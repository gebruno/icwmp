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
#include <fcntl.h>

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

static char *get_value_from_uci_option(struct uci_option *tb)
{
	if (tb == NULL)
		return "";

	if (tb->type == UCI_TYPE_STRING)
		return tb->v.string;

	return "";
}

static void set_cr_incoming_rule(const char *rule)
{
	if (CWMP_LSTRCASECMP(rule, "ip_only") == 0) {
		cwmp_main->cr_policy = CR_POLICY_IP_Only;
	} else if (CWMP_LSTRCASECMP(rule, "ip_port") == 0) {
		cwmp_main->cr_policy = CR_POLICY_BOTH;
	} else {
		cwmp_main->cr_policy = CR_POLICY_Port_Only; // Default case
	}
}

static void config_get_cpe_elements(struct uci_section *s)
{
	enum {
		UCI_CPE_CON_REQ_TIMEOUT,
		UCI_CPE_USER_ID,
		UCI_CPE_PASSWD,
		UCI_CPE_PORT,
		UCI_CPE_CRPATH,
		UCI_CPE_NOTIFY_PERIODIC_ENABLE,
		UCI_CPE_NOTIFY_PERIOD,
		UCI_CPE_SCHEDULE_REBOOT,
		UCI_CPE_DELAY_REBOOT,
		UCI_CPE_ACTIVE_NOTIF_THROTTLE,
		UCI_CPE_MANAGEABLE_DEVICES_NOTIF_LIMIT,
		UCI_CPE_SESSION_TIMEOUT,
		UCI_CPE_INSTANCE_MODE,
		UCI_CPE_JSON_CUSTOM_NOTIFY_FILE,
		UCI_CPE_JSON_FORCED_INFORM_FILE,
		UCI_CPE_FORCE_IPV4,
		UCI_CPE_KEEP_SETTINGS,
		__MAX_NUM_UCI_CPE_ATTRS,
	};

	const struct uci_parse_option cpe_opts[] = {
		[UCI_CPE_USER_ID] = { .name = "userid", .type = UCI_TYPE_STRING },
		[UCI_CPE_PASSWD] = { .name = "passwd", .type = UCI_TYPE_STRING },
		[UCI_CPE_PORT] = { .name = "port", .type = UCI_TYPE_STRING },
		[UCI_CPE_CRPATH] = { .name = "path", .type = UCI_TYPE_STRING },
		[UCI_CPE_NOTIFY_PERIODIC_ENABLE] = { .name = "periodic_notify_enable", .type = UCI_TYPE_STRING },
		[UCI_CPE_NOTIFY_PERIOD] = { .name = "periodic_notify_interval", .type = UCI_TYPE_STRING },
		[UCI_CPE_SCHEDULE_REBOOT] = { .name = "schedule_reboot", .type = UCI_TYPE_STRING },
		[UCI_CPE_DELAY_REBOOT] = { .name = "delay_reboot", .type = UCI_TYPE_STRING },
		[UCI_CPE_ACTIVE_NOTIF_THROTTLE] = { .name = "active_notif_throttle", .type = UCI_TYPE_STRING },
		[UCI_CPE_MANAGEABLE_DEVICES_NOTIF_LIMIT] = { .name = "md_notif_limit", .type = UCI_TYPE_STRING },
		[UCI_CPE_SESSION_TIMEOUT] = { .name = "session_timeout", .type = UCI_TYPE_STRING },
		[UCI_CPE_INSTANCE_MODE] = { .name = "instance_mode", .type = UCI_TYPE_STRING },
		[UCI_CPE_JSON_CUSTOM_NOTIFY_FILE] = { .name = "custom_notify_json", .type = UCI_TYPE_STRING },
		[UCI_CPE_JSON_FORCED_INFORM_FILE] = { .name = "forced_inform_json", .type = UCI_TYPE_STRING },
		[UCI_CPE_CON_REQ_TIMEOUT] = { .name = "cr_timeout", .type = UCI_TYPE_STRING },
		[UCI_CPE_FORCE_IPV4] = { .name = "force_ipv4", .type = UCI_TYPE_STRING },
		[UCI_CPE_KEEP_SETTINGS] = { .name = "keep_settings", .type = UCI_TYPE_STRING }
	};

	struct uci_option *cpe_tb[__MAX_NUM_UCI_CPE_ATTRS];

	CWMP_MEMSET(cpe_tb, 0, sizeof(cpe_tb));
	uci_parse_section(s, cpe_opts, __MAX_NUM_UCI_CPE_ATTRS, cpe_tb);

	snprintf(cwmp_main->conf.cpe_userid, sizeof(cwmp_main->conf.cpe_userid), "%s", get_value_from_uci_option(cpe_tb[UCI_CPE_USER_ID]));
	CWMP_LOG(DEBUG, "CWMP CONFIG - cpe username: %s", cwmp_main->conf.cpe_userid);

	snprintf(cwmp_main->conf.cpe_passwd, sizeof(cwmp_main->conf.cpe_passwd), "%s", get_value_from_uci_option(cpe_tb[UCI_CPE_PASSWD]));
	CWMP_LOG(DEBUG, "CWMP CONFIG - cpe password: %s", cwmp_main->conf.cpe_passwd);

	cwmp_main->conf.cr_timeout = DEFAULT_CR_TIMEOUT;
	char *tm_out = get_value_from_uci_option(cpe_tb[UCI_CPE_CON_REQ_TIMEOUT]);
	if (strlen(tm_out) != 0) {
		int a = strtod(tm_out, NULL);
		if (a > 0) {
			cwmp_main->conf.cr_timeout = a;
		}
	}
	CWMP_LOG(DEBUG, "CWMP CONFIG - cpe connection request timeout: %d", cwmp_main->conf.cr_timeout);

	cwmp_main->conf.connection_request_port = DEFAULT_CONNECTION_REQUEST_PORT;
	char *port = get_value_from_uci_option(cpe_tb[UCI_CPE_PORT]);
	if (strlen(port) != 0) {
		int a = atoi(port);
		cwmp_main->conf.connection_request_port = (a != 0) ? a : DEFAULT_CONNECTION_REQUEST_PORT;
	}
	CWMP_LOG(DEBUG, "CWMP CONFIG - cpe connection request port: %d", cwmp_main->conf.connection_request_port);

	char *crpath = get_value_from_uci_option(cpe_tb[UCI_CPE_CRPATH]);
	snprintf(cwmp_main->conf.connection_request_path, sizeof(cwmp_main->conf.connection_request_path), "%s", strlen(crpath) ? (*crpath == '/') ? crpath + 1 : crpath : "/");
	CWMP_LOG(DEBUG, "CWMP CONFIG - cpe connection request path: %s", cwmp_main->conf.connection_request_path);

	cwmp_main->conf.periodic_notify_enable = uci_str_to_bool(get_value_from_uci_option(cpe_tb[UCI_CPE_NOTIFY_PERIODIC_ENABLE]));
	CWMP_LOG(DEBUG, "CWMP CONFIG - cpe periodic notify enable: %d", cwmp_main->conf.periodic_notify_enable);

	cwmp_main->conf.periodic_notify_interval = DEFAULT_NOTIFY_PERIOD;
	char *notify_period = get_value_from_uci_option(cpe_tb[UCI_CPE_NOTIFY_PERIOD]);
	if (strlen(notify_period) != 0) {
		int a = atoi(notify_period);
		cwmp_main->conf.periodic_notify_interval = (a != 0) ? a : DEFAULT_NOTIFY_PERIOD;
	}
	CWMP_LOG(DEBUG, "CWMP CONFIG - cpe periodic notify interval: %d", cwmp_main->conf.periodic_notify_interval);

	cwmp_main->conf.schedule_reboot = 0;
	char *schedule_reboot = get_value_from_uci_option(cpe_tb[UCI_CPE_SCHEDULE_REBOOT]);
	if (strlen(schedule_reboot) != 0) {
		cwmp_main->conf.schedule_reboot = convert_datetime_to_timestamp(schedule_reboot);
	}

	cwmp_main->conf.delay_reboot = -1;
	char *delay_reboot = get_value_from_uci_option(cpe_tb[UCI_CPE_DELAY_REBOOT]);
	if (strlen(delay_reboot) != 0) {
		int a = atoi(delay_reboot);
		cwmp_main->conf.delay_reboot = (a > 0) ? a : -1;
	}

	cwmp_main->conf.active_notif_throttle = 0;
	char *notify_thottle = get_value_from_uci_option(cpe_tb[UCI_CPE_ACTIVE_NOTIF_THROTTLE]);
	if (strlen(notify_thottle) != 0) {
		int a = atoi(notify_thottle);
		cwmp_main->conf.active_notif_throttle = (a > 0) ? a : 0;
	}

	cwmp_main->conf.md_notif_limit = 0;
	char *notify_limit = get_value_from_uci_option(cpe_tb[UCI_CPE_MANAGEABLE_DEVICES_NOTIF_LIMIT]);
	if (strlen(notify_limit) != 0) {
		int a = atoi(notify_limit);
		cwmp_main->conf.md_notif_limit = (a > 0) ? a : 0;
	}

	cwmp_main->conf.session_timeout = DEFAULT_SESSION_TIMEOUT;
	char *session_timeout = get_value_from_uci_option(cpe_tb[UCI_CPE_SESSION_TIMEOUT]);
	if (strlen(session_timeout) != 0) {
		int a = atoi(session_timeout);
		cwmp_main->conf.session_timeout = (a >= 1) ? a : DEFAULT_SESSION_TIMEOUT;
	}

	cwmp_main->conf.instance_mode = DEFAULT_INSTANCE_MODE;
	char *instance_mode = get_value_from_uci_option(cpe_tb[UCI_CPE_INSTANCE_MODE]);
	if (strlen(instance_mode) != 0) {
		if (CWMP_STRCMP(instance_mode, "InstanceNumber") == 0) {
			cwmp_main->conf.instance_mode = INSTANCE_MODE_NUMBER;
		} else {
			cwmp_main->conf.instance_mode = INSTANCE_MODE_ALIAS;
		}
	}

	snprintf(cwmp_main->conf.custom_notify_json, sizeof(cwmp_main->conf.custom_notify_json), "%s", get_value_from_uci_option(cpe_tb[UCI_CPE_JSON_CUSTOM_NOTIFY_FILE]));
	CWMP_LOG(DEBUG, "CWMP CONFIG - cpe custom notify json path: %s", cwmp_main->conf.custom_notify_json);

	snprintf(cwmp_main->conf.forced_inform_json, sizeof(cwmp_main->conf.forced_inform_json), "%s", get_value_from_uci_option(cpe_tb[UCI_CPE_JSON_FORCED_INFORM_FILE]));
	CWMP_LOG(DEBUG, "CWMP CONFIG - cpe forced inform json path: %s", cwmp_main->conf.forced_inform_json);

	cwmp_main->conf.force_ipv4 = uci_str_to_bool(get_value_from_uci_option(cpe_tb[UCI_CPE_FORCE_IPV4]));
	CWMP_LOG(DEBUG, "CWMP CONFIG - cpe force ipv4 enable: %d", cwmp_main->conf.force_ipv4);

	cwmp_main->conf.keep_settings = cpe_tb[UCI_CPE_KEEP_SETTINGS] ? uci_str_to_bool(get_value_from_uci_option(cpe_tb[UCI_CPE_KEEP_SETTINGS])) : true;
	CWMP_LOG(DEBUG, "CWMP CONFIG - cpe keep settings enable: %d", cwmp_main->conf.keep_settings);
}

static void config_get_acs_elements(struct uci_section *s)
{
	enum {
		UCI_ACS_SSL_CAPATH,
		UCI_ACS_HTTP_DISABLE_100CONTINUE,
		UCI_ACS_INSECURE_ENABLE,
		UCI_ACS_DHCP_DISCOVERY,
		UCI_ACS_URL,
		UCI_ACS_DHCP_URL,
		UCI_ACS_USERID,
		UCI_ACS_PASSWR,
		UCI_ACS_RETRY_MIN_WAIT_INTERVAL,
		UCI_ACS_DHCP_RETRY_MIN_WAIT_INTERVAL,
		UCI_ACS_RETRY_INTERVAL_MULTIPLIER,
		UCI_ACS_DHCP_RETRY_INTERVAL_MULTIPLIER,
		UCI_ACS_COMPRESSION,
		UCI_ACS_GETRPC,
		UCI_ACS_PERIODIC_INFORM_TIME,
		UCI_ACS_PERIODIC_INFORM_INTERVAL,
		UCI_ACS_PERIODIC_INFORM_ENABLE,
		UCI_ACS_HEARTBEAT_ENABLE,
		UCI_ACS_HEARTBEAT_INTERVAL,
		UCI_ACS_HEARTBEAT_TIME,
		__MAX_NUM_UCI_ACS_ATTRS,
	};

	const struct uci_parse_option acs_opts[] = {
		[UCI_ACS_SSL_CAPATH] = { .name = "ssl_capath", .type = UCI_TYPE_STRING },
		[UCI_ACS_HTTP_DISABLE_100CONTINUE] = { .name = "http_disable_100continue", .type = UCI_TYPE_STRING },
		[UCI_ACS_INSECURE_ENABLE] = { .name = "insecure_enable", .type = UCI_TYPE_STRING },
		[UCI_ACS_DHCP_DISCOVERY] = { .name = "dhcp_discovery", .type = UCI_TYPE_STRING },
		[UCI_ACS_URL] = { .name = "url", .type = UCI_TYPE_STRING },
		[UCI_ACS_DHCP_URL] = { .name = "dhcp_url", .type = UCI_TYPE_STRING },
		[UCI_ACS_USERID] = { .name = "userid", .type = UCI_TYPE_STRING },
		[UCI_ACS_PASSWR] = { .name = "passwd", .type = UCI_TYPE_STRING },
		[UCI_ACS_RETRY_MIN_WAIT_INTERVAL] = { .name = "retry_min_wait_interval", .type = UCI_TYPE_STRING },
		[UCI_ACS_DHCP_RETRY_MIN_WAIT_INTERVAL] = { .name = "dhcp_retry_min_wait_interval", .type = UCI_TYPE_STRING },
		[UCI_ACS_RETRY_INTERVAL_MULTIPLIER] = { .name = "retry_interval_multiplier", .type = UCI_TYPE_STRING },
		[UCI_ACS_DHCP_RETRY_INTERVAL_MULTIPLIER] = { .name = "dhcp_retry_interval_multiplier", .type = UCI_TYPE_STRING },
		[UCI_ACS_COMPRESSION] = { .name = "compression", .type = UCI_TYPE_STRING },
		[UCI_ACS_GETRPC] = { .name = "get_rpc_methods", .type = UCI_TYPE_STRING },
		[UCI_ACS_PERIODIC_INFORM_TIME] = { .name = "periodic_inform_time", .type = UCI_TYPE_STRING },
		[UCI_ACS_PERIODIC_INFORM_INTERVAL] = { .name = "periodic_inform_interval", .type = UCI_TYPE_STRING },
		[UCI_ACS_PERIODIC_INFORM_ENABLE] = { .name = "periodic_inform_enable", .type = UCI_TYPE_STRING },
		[UCI_ACS_HEARTBEAT_ENABLE] = { .name = "heartbeat_enable", .type = UCI_TYPE_STRING },
		[UCI_ACS_HEARTBEAT_INTERVAL] = { .name = "heartbeat_interval", .type = UCI_TYPE_STRING },
		[UCI_ACS_HEARTBEAT_TIME] = { .name = "heartbeat_time", .type = UCI_TYPE_STRING },
	};

	struct uci_option *acs_tb[__MAX_NUM_UCI_ACS_ATTRS];

	CWMP_MEMSET(acs_tb, 0, sizeof(acs_tb));
	uci_parse_section(s, acs_opts, __MAX_NUM_UCI_ACS_ATTRS, acs_tb);

	cwmp_main->conf.http_disable_100continue = uci_str_to_bool(get_value_from_uci_option(acs_tb[UCI_ACS_HTTP_DISABLE_100CONTINUE]));
	CWMP_LOG(DEBUG, "CWMP CONFIG - acs http disable 100continue: %d", cwmp_main->conf.http_disable_100continue);

	cwmp_main->conf.insecure_enable = uci_str_to_bool(get_value_from_uci_option(acs_tb[UCI_ACS_INSECURE_ENABLE]));
	CWMP_LOG(DEBUG, "CWMP CONFIG - acs insecure enable: %d", cwmp_main->conf.insecure_enable);

	cwmp_main->conf.dhcp_discovery = uci_str_to_bool(get_value_from_uci_option(acs_tb[UCI_ACS_DHCP_DISCOVERY]));
	CWMP_LOG(DEBUG, "CWMP CONFIG - acs dhcp discovery: %d", cwmp_main->conf.dhcp_discovery);

	char *get_rpc = get_value_from_uci_option(acs_tb[UCI_ACS_GETRPC]);
	cwmp_main->conf.acs_getrpc = CWMP_STRLEN(get_rpc) ? uci_str_to_bool(get_rpc) : true;
	CWMP_LOG(DEBUG, "CWMP CONFIG - acs get rpc: %d", cwmp_main->conf.acs_getrpc);

	char *url = get_value_from_uci_option(acs_tb[UCI_ACS_URL]);
	char *dhcp_url = get_value_from_uci_option(acs_tb[UCI_ACS_DHCP_URL]);
	snprintf(cwmp_main->conf.acs_url, sizeof(cwmp_main->conf.acs_url), "%s", cwmp_main->conf.dhcp_discovery ? (strlen(dhcp_url) ? dhcp_url : url) : url);
	CWMP_LOG(DEBUG, "CWMP CONFIG - acs url: %s", cwmp_main->conf.acs_url);

	snprintf(cwmp_main->conf.acs_userid, sizeof(cwmp_main->conf.acs_userid), "%s", get_value_from_uci_option(acs_tb[UCI_ACS_USERID]));
	CWMP_LOG(DEBUG, "CWMP CONFIG - acs username: %s", cwmp_main->conf.acs_userid);

	snprintf(cwmp_main->conf.acs_passwd, sizeof(cwmp_main->conf.acs_passwd), "%s", get_value_from_uci_option(acs_tb[UCI_ACS_PASSWR]));
	CWMP_LOG(DEBUG, "CWMP CONFIG - acs password: %s", cwmp_main->conf.acs_passwd);

	snprintf(cwmp_main->conf.acs_ssl_capath, sizeof(cwmp_main->conf.acs_ssl_capath), "%s", get_value_from_uci_option(acs_tb[UCI_ACS_SSL_CAPATH]));
	CWMP_LOG(DEBUG, "CWMP CONFIG - acs ssl capath: %s", cwmp_main->conf.acs_ssl_capath);

	cwmp_main->conf.retry_min_wait_interval = DEFAULT_RETRY_MINIMUM_WAIT_INTERVAL;
	char *acs_retry_min_wait_interval = get_value_from_uci_option(acs_tb[UCI_ACS_RETRY_MIN_WAIT_INTERVAL]);
	char *acs_dhcp_retry_min_wait_interval = get_value_from_uci_option(acs_tb[UCI_ACS_DHCP_RETRY_MIN_WAIT_INTERVAL]);
	char *op_interval = cwmp_main->conf.dhcp_discovery ? acs_dhcp_retry_min_wait_interval : acs_retry_min_wait_interval;
	if (strlen(op_interval) != 0) {
		if (cwmp_main->conf.amd_version >= AMD_3) {
			int a = atoi(op_interval);
			cwmp_main->conf.retry_min_wait_interval = (a <= 65535 && a >= 1) ? a : DEFAULT_RETRY_MINIMUM_WAIT_INTERVAL;
		}
	}
	CWMP_LOG(DEBUG, "CWMP CONFIG - acs retry minimum wait interval: %d", cwmp_main->conf.retry_min_wait_interval);

	cwmp_main->conf.retry_interval_multiplier = DEFAULT_RETRY_INTERVAL_MULTIPLIER;
	char *acs_retry_interval_multiplier = get_value_from_uci_option(acs_tb[UCI_ACS_RETRY_INTERVAL_MULTIPLIER]);
	char *acs_dhcp_retry_interval_multiplier = get_value_from_uci_option(acs_tb[UCI_ACS_DHCP_RETRY_INTERVAL_MULTIPLIER]);
	char *op_multi = cwmp_main->conf.dhcp_discovery ? acs_dhcp_retry_interval_multiplier : acs_retry_interval_multiplier;
	if (strlen(op_multi) != 0) {
		if (cwmp_main->conf.amd_version >= AMD_3) {
			int a = atoi(op_multi);
			cwmp_main->conf.retry_interval_multiplier = (a <= 65535 && a >= 1000) ? a : DEFAULT_RETRY_INTERVAL_MULTIPLIER;
		}
	}
	CWMP_LOG(DEBUG, "CWMP CONFIG - acs retry interval multiplier: %d", cwmp_main->conf.retry_interval_multiplier);

	cwmp_main->conf.compression = COMP_NONE;
	char *acs_comp = get_value_from_uci_option(acs_tb[UCI_ACS_COMPRESSION]);
	if (cwmp_main->conf.amd_version >= AMD_5 && strlen(acs_comp) != 0) {
		if (strcasecmp(acs_comp, "gzip") == 0) {
			cwmp_main->conf.compression = COMP_GZIP;
		} else if (strcasecmp(acs_comp, "deflate") == 0) {
			cwmp_main->conf.compression = COMP_DEFLATE;
		} else {
			cwmp_main->conf.compression = COMP_NONE;
		}
	}

	cwmp_main->conf.time = 0;
	char *time = get_value_from_uci_option(acs_tb[UCI_ACS_PERIODIC_INFORM_TIME]);
	if (strlen(time) != 0) {
		cwmp_main->conf.time = convert_datetime_to_timestamp(time);
	}

	cwmp_main->conf.period = PERIOD_INFORM_DEFAULT;
	char *inform_interval = get_value_from_uci_option(acs_tb[UCI_ACS_PERIODIC_INFORM_INTERVAL]);
	if (strlen(inform_interval) != 0) {
		int a = atoi(inform_interval);
		cwmp_main->conf.period = (a >= PERIOD_INFORM_MIN) ? a : PERIOD_INFORM_DEFAULT;
	}
	CWMP_LOG(DEBUG, "CWMP CONFIG - acs periodic inform: %d", cwmp_main->conf.period);

	cwmp_main->conf.periodic_enable = uci_str_to_bool(get_value_from_uci_option(acs_tb[UCI_ACS_PERIODIC_INFORM_ENABLE]));
	CWMP_LOG(DEBUG, "CWMP CONFIG - acs periodic enable: %d", cwmp_main->conf.periodic_enable);

	cwmp_main->conf.heart_beat_enable = uci_str_to_bool(get_value_from_uci_option(acs_tb[UCI_ACS_HEARTBEAT_ENABLE]));
	CWMP_LOG(DEBUG, "CWMP CONFIG - acs heart beat enable: %d", cwmp_main->conf.heart_beat_enable);

	cwmp_main->conf.heartbeat_interval = 30;
	char *heartbeat_interval = get_value_from_uci_option(acs_tb[UCI_ACS_HEARTBEAT_INTERVAL]);
	if (strlen(heartbeat_interval) != 0) {
		int a = atoi(heartbeat_interval);
		cwmp_main->conf.heartbeat_interval = a;
	}
	CWMP_LOG(DEBUG, "CWMP CONFIG - acs heartbeat interval: %d", cwmp_main->conf.heartbeat_interval);

	cwmp_main->conf.heart_time = 0;
	char *heartbeat_time = get_value_from_uci_option(acs_tb[UCI_ACS_HEARTBEAT_TIME]);
	if (strlen(heartbeat_time) != 0) {
		cwmp_main->conf.heart_time = convert_datetime_to_timestamp(heartbeat_time);
	}
}

static void config_get_lwn_elements(struct uci_section *s)
{
	enum {
		UCI_LWN_ENABLE,
		UCI_LWN_HOSTNAME,
		UCI_LWN_PORT,
		__MAX_NUM_UCI_LWN_ATTRS,
	};

	const struct uci_parse_option acs_opts[] = {
		[UCI_LWN_ENABLE] = { .name = "enable", .type = UCI_TYPE_STRING },
		[UCI_LWN_HOSTNAME] = { .name = "hostname", .type = UCI_TYPE_STRING },
		[UCI_LWN_PORT] = { .name = "port", .type = UCI_TYPE_STRING },
	};

	struct uci_option *lwn_tb[__MAX_NUM_UCI_LWN_ATTRS];

	CWMP_MEMSET(lwn_tb, 0, sizeof(lwn_tb));
	uci_parse_section(s, acs_opts, __MAX_NUM_UCI_LWN_ATTRS, lwn_tb);

	cwmp_main->conf.lwn_enable = uci_str_to_bool(get_value_from_uci_option(lwn_tb[UCI_LWN_ENABLE]));
	CWMP_LOG(DEBUG, "CWMP CONFIG - lwn enable: %d", cwmp_main->conf.lwn_enable);

	snprintf(cwmp_main->conf.lwn_hostname, sizeof(cwmp_main->conf.lwn_hostname), "%s", get_value_from_uci_option(lwn_tb[UCI_LWN_HOSTNAME]));
	CWMP_LOG(DEBUG, "CWMP CONFIG - lwn hostname: %s", cwmp_main->conf.lwn_hostname);

	cwmp_main->conf.lwn_port = DEFAULT_LWN_PORT;
	char *port = get_value_from_uci_option(lwn_tb[UCI_LWN_PORT]);
	if (strlen(port) != 0) {
		int a = atoi(port);
		cwmp_main->conf.lwn_port = a;
	}
	CWMP_LOG(DEBUG, "CWMP CONFIG - lwn port: %d", cwmp_main->conf.lwn_port);
}

static void config_get_tc_elements(struct uci_section *s)
{
	enum {
		UCI_TC_ENABLE,
		UCI_TC_TRANSFERTYPE,
		UCI_TC_RESULTTYPE,
		UCI_TC_FILETYPE,
		__MAX_NUM_UCI_TC_ATTRS,
	};

	const struct uci_parse_option acs_opts[] = {
		[UCI_TC_ENABLE] = { .name = "enable", .type = UCI_TYPE_STRING },
		[UCI_TC_TRANSFERTYPE] = { .name = "transfer_type", .type = UCI_TYPE_STRING },
		[UCI_TC_RESULTTYPE] = { .name = "result_type", .type = UCI_TYPE_STRING },
		[UCI_TC_FILETYPE] = { .name = "file_type", .type = UCI_TYPE_STRING },
	};

	struct uci_option *tc_tb[__MAX_NUM_UCI_TC_ATTRS];

	CWMP_MEMSET(tc_tb, 0, sizeof(tc_tb));
	uci_parse_section(s, acs_opts, __MAX_NUM_UCI_TC_ATTRS, tc_tb);

	cwmp_main->conf.auto_tc_enable = uci_str_to_bool(get_value_from_uci_option(tc_tb[UCI_TC_ENABLE]));
	CWMP_LOG(DEBUG, "CWMP CONFIG - tc enable: %d", cwmp_main->conf.auto_tc_enable);

	snprintf(cwmp_main->conf.auto_tc_transfer_type, sizeof(cwmp_main->conf.auto_tc_transfer_type), "%s", get_value_from_uci_option(tc_tb[UCI_TC_TRANSFERTYPE]));
	CWMP_LOG(DEBUG, "CWMP CONFIG - tc transfer type: %s", cwmp_main->conf.auto_tc_transfer_type);

	snprintf(cwmp_main->conf.auto_tc_result_type, sizeof(cwmp_main->conf.auto_tc_result_type), "%s", get_value_from_uci_option(tc_tb[UCI_TC_RESULTTYPE]));
	CWMP_LOG(DEBUG, "CWMP CONFIG - tc result type: %s", cwmp_main->conf.auto_tc_result_type);

	snprintf(cwmp_main->conf.auto_tc_file_type, sizeof(cwmp_main->conf.auto_tc_file_type), "%s", get_value_from_uci_option(tc_tb[UCI_TC_FILETYPE]));
	CWMP_LOG(DEBUG, "CWMP CONFIG - tc file type: %s", cwmp_main->conf.auto_tc_file_type);
}

static void config_get_cds_elements(struct uci_section *s)
{
	enum {
		UCI_CDS_ENABLE,
		UCI_CDS_OPTYPE,
		UCI_CDS_RESULTYPE,
		UCI_CDS_FAULTCODE,
		__MAX_NUM_UCI_CDS_ATTRS,
	};

	const struct uci_parse_option cdu_opts[] = {
		[UCI_CDS_ENABLE] = { .name = "enable", .type = UCI_TYPE_STRING },
		[UCI_CDS_OPTYPE] = { .name = "operation_type", .type = UCI_TYPE_STRING },
		[UCI_CDS_RESULTYPE] = { .name = "result_type", .type = UCI_TYPE_STRING },
		[UCI_CDS_FAULTCODE] = { .name = "fault_code", .type = UCI_TYPE_STRING },
	};

	struct uci_option *cds_tb[__MAX_NUM_UCI_CDS_ATTRS];

	CWMP_MEMSET(cds_tb, 0, sizeof(cds_tb));
	uci_parse_section(s, cdu_opts, __MAX_NUM_UCI_CDS_ATTRS, cds_tb);

	cwmp_main->conf.auto_cdu_enable = uci_str_to_bool(get_value_from_uci_option(cds_tb[UCI_CDS_ENABLE]));
	CWMP_LOG(DEBUG, "CWMP CONFIG - cds enable: %d", cwmp_main->conf.auto_cdu_enable);

	snprintf(cwmp_main->conf.auto_cdu_oprt_type, sizeof(cwmp_main->conf.auto_cdu_oprt_type), "%s", get_value_from_uci_option(cds_tb[UCI_CDS_OPTYPE]));
	CWMP_LOG(DEBUG, "CWMP CONFIG - cds operation type: %s", cwmp_main->conf.auto_cdu_oprt_type);

	snprintf(cwmp_main->conf.auto_cdu_result_type, sizeof(cwmp_main->conf.auto_cdu_result_type), "%s", get_value_from_uci_option(cds_tb[UCI_CDS_RESULTYPE]));
	CWMP_LOG(DEBUG, "CWMP CONFIG - cds result type: %s", cwmp_main->conf.auto_cdu_result_type);

	snprintf(cwmp_main->conf.auto_cdu_fault_code, sizeof(cwmp_main->conf.auto_cdu_fault_code), "%s", get_value_from_uci_option(cds_tb[UCI_CDS_FAULTCODE]));
	CWMP_LOG(DEBUG, "CWMP CONFIG - cds fault type: %s", cwmp_main->conf.auto_cdu_fault_code);
}

static void configure_var_state(void)
{
	if (!file_exists(VARSTATE_CONFIG"/icwmp"))
		creat(VARSTATE_CONFIG"/icwmp", S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

	cwmp_uci_add_section_with_specific_name("icwmp", "acs", "acs", UCI_VARSTATE_CONFIG);
	cwmp_uci_add_section_with_specific_name("icwmp", "cpe", "cpe", UCI_VARSTATE_CONFIG);

	cwmp_commit_package("icwmp", UCI_VARSTATE_CONFIG);
}

int get_preinit_config()
{
#define UCI_CPE_LOG_FILE_NAME "cwmp.cpe.log_file_name"
#define UCI_CPE_LOG_MAX_SIZE "cwmp.cpe.log_max_size"
#define UCI_CPE_ENABLE_STDOUT_LOG "cwmp.cpe.log_to_console"
#define UCI_CPE_ENABLE_FILE_LOG "cwmp.cpe.log_to_file"
#define UCI_LOG_SEVERITY_PATH "cwmp.cpe.log_severity"
#define UCI_CPE_ENABLE_SYSLOG "cwmp.cpe.log_to_syslog"
#define UCI_CPE_DEFAULT_WAN_IFACE "cwmp.cpe.default_wan_interface"
#define UCI_CPE_INCOMING_RULE "cwmp.cpe.incoming_rule"
#define UCI_CPE_AMD_VERSION "cwmp.cpe.amd_version"

	char *value = NULL;

	cwmp_uci_init();

	uci_get_value(UCI_LOG_SEVERITY_PATH, &value);
	log_set_severity_idx(value);
	FREE(value);

	uci_get_value(UCI_CPE_LOG_FILE_NAME, &value);
	log_set_log_file_name(value);
	FREE(value);

	uci_get_value(UCI_CPE_LOG_MAX_SIZE, &value);
	log_set_file_max_size(value);
	FREE(value);

	uci_get_value(UCI_CPE_ENABLE_STDOUT_LOG, &value);
	log_set_on_console(value);
	FREE(value);

	uci_get_value(UCI_CPE_ENABLE_FILE_LOG, &value);
	log_set_on_file(value);
	FREE(value);

	uci_get_value(UCI_CPE_ENABLE_SYSLOG, &value);
	log_set_on_syslog(value);
	FREE(value);

	uci_get_value(UCI_CPE_DEFAULT_WAN_IFACE, &value);
	snprintf(cwmp_main->conf.default_wan_iface, sizeof(cwmp_main->conf.default_wan_iface), "%s", value ? value : "wan");
	FREE(value);

	uci_get_value(UCI_CPE_INCOMING_RULE, &value);
	set_cr_incoming_rule(value);
	FREE(value);

	cwmp_main->conf.amd_version = DEFAULT_AMD_VERSION;
	uci_get_value(UCI_CPE_AMD_VERSION, &value);
	if (CWMP_STRLEN(value) != 0) {
		int a = atoi(value);
		cwmp_main->conf.amd_version = (a >= 1 && a <= 6) ? a : DEFAULT_AMD_VERSION;
		FREE(value);
	}

	cwmp_main->conf.supported_amd_version = cwmp_main->conf.amd_version;

	configure_var_state();

	cwmp_uci_exit();

	CWMP_LOG(DEBUG, "CWMP CONFIG - default wan interface: %s", cwmp_main->conf.default_wan_iface);
	CWMP_LOG(DEBUG, "CWMP CONFIG - amendement version: %d", cwmp_main->conf.amd_version);

	return CWMP_OK;
}


int get_global_config()
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

		if (CWMP_STRCMP(s->type, "acs") == 0) {
			config_get_acs_elements(s);
		} else if (CWMP_STRCMP(s->type, "cpe") == 0) {
			config_get_cpe_elements(s);
		} else if (CWMP_STRCMP(s->type, "lwn") == 0) {
			config_get_lwn_elements(s);
		} else if (CWMP_STRCMP(s->type, "transfer_complete") == 0) {
			config_get_tc_elements(s);
		} else if (CWMP_STRCMP(s->type, "du_state_change") == 0) {
			config_get_cds_elements(s);
		}
	}

	uci_free_context(ctx);
	return CWMP_OK;
}

int global_conf_init()
{
	int error = CWMP_OK;

	pthread_mutex_lock(&mutex_config_load);

	if ((error = get_global_config())) {
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
	snprintf(cwmp_main->deviceid.manufacturer, sizeof(cwmp_main->deviceid.manufacturer), "%s", dm_param.value ? dm_param.value : "");

	cwmp_get_parameter_value("Device.DeviceInfo.SerialNumber", &dm_param);
	snprintf(cwmp_main->deviceid.serialnumber, sizeof(cwmp_main->deviceid.serialnumber), "%s", dm_param.value ? dm_param.value : "");

	cwmp_get_parameter_value("Device.DeviceInfo.ProductClass", &dm_param);
	snprintf(cwmp_main->deviceid.productclass, sizeof(cwmp_main->deviceid.productclass), "%s", dm_param.value ? dm_param.value : "");

	cwmp_get_parameter_value("Device.DeviceInfo.ManufacturerOUI", &dm_param);
	snprintf(cwmp_main->deviceid.oui, sizeof(cwmp_main->deviceid.oui), "%s", dm_param.value ? dm_param.value : "");

	cwmp_get_parameter_value("Device.DeviceInfo.SoftwareVersion", &dm_param);
	snprintf(cwmp_main->deviceid.softwareversion, sizeof(cwmp_main->deviceid.softwareversion), "%s", dm_param.value ? dm_param.value : "");

	return CWMP_OK;
}

int cwmp_config_reload()
{
	CWMP_MEMSET(&cwmp_main->env, 0, sizeof(struct env));

	int err = global_conf_init();
	if (err != CWMP_OK)
		return err;

	return CWMP_OK;
}
