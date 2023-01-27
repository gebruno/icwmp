/*
 * cwmp_uci.h - API to manage UCI packages/sections/options
 *
 * Copyright (C) 2021-2022, IOPSYS Software Solutions AB.
 *
 *	  Author Omar Kallel <omar.kallel@pivasoftware.com>
 *
 * See LICENSE file for license related information.
 *
 */


#ifndef __CWMPUCI_H
#define __CWMPUCI_H

#include <uci.h>
#include <libubox/list.h>


//struct uci_context *cwmp_uci_ctx = ((void *)0);
#define UCI_DHCP_DISCOVERY_PATH "cwmp.acs.dhcp_discovery"
#define UCI_ACS_URL_PATH "cwmp.acs.url"
#define UCI_PERIODIC_INFORM_TIME_PATH "cwmp.acs.periodic_inform_time"
#define UCI_PERIODIC_INFORM_INTERVAL_PATH "cwmp.acs.periodic_inform_interval"
#define UCI_PERIODIC_INFORM_ENABLE_PATH "cwmp.acs.periodic_inform_enable"
#define UCI_ACS_USERID_PATH "cwmp.acs.userid"
#define UCI_ACS_PASSWD_PATH "cwmp.acs.passwd"
#define UCI_ACS_COMPRESSION "cwmp.acs.compression"
#define UCI_ACS_RETRY_MIN_WAIT_INTERVAL "cwmp.acs.retry_min_wait_interval"
#define UCI_ACS_RETRY_INTERVAL_MULTIPLIER "cwmp.acs.retry_interval_multiplier"
#define UCI_ACS_GETRPC "cwmp.acs.get_rpc_methods"
#define UCI_CPE_USERID_PATH "cwmp.cpe.userid"
#define UCI_CPE_PASSWD_PATH "cwmp.cpe.passwd"
#define UCI_CPE_CWMP_ENABLE "cwmp.cpe.enable"
#define UCI_CPE_PORT_PATH "cwmp.cpe.port"
#define UCI_CPE_CRPATH_PATH "cwmp.cpe.path"
#define UCI_CPE_INSTANCE_MODE "cwmp.cpe.instance_mode"
#define UCI_CPE_SESSION_TIMEOUT "cwmp.cpe.session_timeout"
#define UCI_CPE_EXEC_DOWNLOAD "cwmp.cpe.exec_download"
#define UCI_CPE_NOTIFY_PERIODIC_ENABLE "cwmp.cpe.periodic_notify_enable"
#define UCI_CPE_NOTIFY_PERIOD "cwmp.cpe.periodic_notify_interval"
#define UCI_CPE_SCHEDULE_REBOOT "cwmp.cpe.schedule_reboot"
#define UCI_CPE_DELAY_REBOOT "cwmp.cpe.delay_reboot"
#define UCI_CPE_JSON_CUSTOM_NOTIFY_FILE "cwmp.cpe.custom_notify_json"
#define LW_NOTIFICATION_ENABLE "cwmp.lwn.enable"
#define LW_NOTIFICATION_HOSTNAME "cwmp.lwn.hostname"
#define LW_NOTIFICATION_PORT "cwmp.lwn.port"
#define UCI_DHCP_ACS_URL "cwmp.acs.dhcp_url"
#define UCI_ACS_HEARTBEAT_ENABLE "cwmp.acs.heartbeat_enable"
#define UCI_ACS_HEARTBEAT_INTERVAL "cwmp.acs.heartbeat_interval"
#define UCI_ACS_HEARTBEAT_TIME "cwmp.acs.heartbeat_time"
#define UCI_DHCP_CPE_PROV_CODE "cwmp.cpe.dhcp_provisioning_code"
#define UCI_DHCP_ACS_RETRY_MIN_WAIT_INTERVAL "cwmp.acs.dhcp_retry_min_wait_interval"
#define UCI_DHCP_ACS_RETRY_INTERVAL_MULTIPLIER "cwmp.acs.dhcp_retry_interval_multiplier"
#define UCI_AUTONOMOUS_DU_STATE_ENABLE "cwmp.du_state_change.enable"
#define UCI_AUTONOMOUS_DU_STATE_OPERATION "cwmp.du_state_change.operation_type"
#define UCI_AUTONOMOUS_DU_STATE_RESULT "cwmp.du_state_change.result_type"
#define UCI_CPE_FIREWALL_RESTART_STATE "cwmp.cpe.firewall_restart"

#define UCI_AUTONOMOUS_TC_ENABLE "cwmp.transfer_complete.enable"
#define UCI_AUTONOMOUS_TC_TRANSFERTYPE "cwmp.transfer_complete.transfer_type"
#define UCI_AUTONOMOUS_TC_RESULTTYPE "cwmp.transfer_complete.result_type"
#define UCI_AUTONOMOUS_TC_FILETYPE "cwmp.transfer_complete.file_type"
#define UCI_AUTONOMOUS_CDU_ENABLE "cwmp.du_state_change.enable"
#define UCI_AUTONOMOUS_CDU_OPTYPE "cwmp.du_state_change.operation_type"
#define UCI_AUTONOMOUS_CDU_RESULTYPE "cwmp.du_state_change.result_type"
#define UCI_AUTONOMOUS_CDU_FAULTCODE "cwmp.du_state_change.fault_code"

#define UCI_CONFIG_DIR "/etc/config/"
#define LIB_DB_CONFIG "/lib/db/config"
#define ETC_DB_CONFIG "/etc/board-db/config"
#define VARSTATE_CONFIG "/var/state"
#define DHCP_OPTION_READ_MAX_RETRY 5
#define UCI_OPTION_READ_INTERVAL 5

#define section_name(s) s ? (s)->e.name : ""
typedef enum uci_config_paths
{
	UCI_STANDARD_CONFIG,
	UCI_VARSTATE_CONFIG,
	UCI_ETCICWMPD_CONFIG
}uci_config_paths;

enum uci_val_type
{
	UCI_INT,
	UCI_STRING
};

union mult_uci_value {
	int int_value;
	char *str_value;
};

struct cwmp_uci_value {
	union mult_uci_value value;
	enum uci_val_type type;
};

#define CWMP_UCI_ARG (struct cwmp_uci_value)
enum cwmp_uci_cmp
{
	CWMP_CMP_SECTION,
	CWMP_CMP_OPTION_EQUAL,
	CWMP_CMP_OPTION_REGEX,
	CWMP_CMP_OPTION_CONTAINING,
	CWMP_CMP_OPTION_CONT_WORD,
	CWMP_CMP_LIST_CONTAINING,
	CWMP_CMP_FILTER_FUNC
};

enum cwmp_uci_walk
{
	CWMP_GET_FIRST_SECTION,
	CWMP_GET_NEXT_SECTION
};

struct config_uci_list {
	struct list_head list;
	char *value;
};

struct uci_paths {
	char *conf_dir;
	char *save_dir;
	struct uci_context *uci_ctx;
};

extern struct uci_paths uci_save_conf_paths[];
int cwmp_uci_init();
void cwmp_uci_exit(void);
void cwmp_uci_reinit(void);
int cwmp_uci_lookup_ptr(struct uci_context *ctx, struct uci_ptr *ptr, char *package, char *section, char *option, char *value);
int cwmp_uci_get_option_value_list(char *package, char *section, char *option, uci_config_paths uci_type, struct uci_list **value);
int cwmp_uci_get_cwmp_standard_option_value_list(char *package, char *section, char *option, struct uci_list **value);
int cwmp_uci_get_cwmp_varstate_option_value_list(char *package, char *section, char *option, struct uci_list **value);
int uci_get_state_value(char *cmd, char **value);
int uci_set_value_by_path(char *cmd, char *value, uci_config_paths uci_type);
int cwmp_uci_set_value_by_path(char *path, char *value);
int cwmp_uci_set_varstate_value_by_path(char *path, char *value);
int uci_get_value(char *cmd, char **value);
struct uci_section *cwmp_uci_walk_section(char *package, char *stype, void *arg1, void *arg2, int cmp, int (*filter)(struct uci_section *s, void *value), struct uci_section *prev_section, uci_config_paths uci_type, int walk);
int cwmp_uci_get_value_by_section_string(struct uci_section *s, char *option, char **value);
int cwmp_uci_get_option_value_string(char *package, char *section, char *option, uci_config_paths uci_type, char **value);
int cwmp_commit_package(char *package, uci_config_paths uci_type);
int cwmp_uci_import(char *package_name, const char *input_path, uci_config_paths uci_type);
int cwmp_uci_export_package(char *package, const char *output_path, uci_config_paths uci_type);
int cwmp_uci_export(const char *output_path, uci_config_paths uci_type);
void cwmp_free_uci_list(struct uci_list *list);
int cwmp_uci_add_list_value(char *package, char *section, char *option, char *value, uci_config_paths uci_type);
int cwmp_uci_del_list_value(char *package, char *section, char *option, char *value, uci_config_paths uci_type);
int cwmp_uci_get_section_type(char *package, char *section, uci_config_paths uci_type, char **value);
int cwmp_uci_add_section(char *package, char *stype, uci_config_paths uci_type, struct uci_section **s);
int cwmp_uci_set_value(char *package, char *section, char *option, char *value);
int cwmp_uci_set_varstate_value(char *package, char*section, char *option, char *value);
int cwmp_uci_add_section_with_specific_name(char *package, char *stype, char *section, uci_config_paths uci_type);
char *cwmp_uci_list_to_string(struct uci_list *list, char *delimitor);
void cwmp_uci_list_init(struct uci_list *ptr);
void cwmp_uci_list_add(struct uci_list *head, struct uci_list *ptr);
struct uci_section* get_section_by_section_name(char *package, char *stype, char* sname, uci_config_paths uci_type);

#define cwmp_uci_path_foreach_option_eq(package, stype, option, val, section) \
	for (section = cwmp_uci_walk_section(package, stype, option, val, CWMP_CMP_OPTION_EQUAL, NULL, NULL, UCI_STANDARD_CONFIG, CWMP_GET_FIRST_SECTION); section != NULL; section = cwmp_uci_walk_section(package, stype, option, val, CWMP_CMP_OPTION_EQUAL, NULL, section, UCI_STANDARD_CONFIG, CWMP_GET_NEXT_SECTION))

#define cwmp_uci_foreach_sections(package, stype, uci_type, section) \
	for (section = cwmp_uci_walk_section(package, stype, NULL, NULL, CWMP_CMP_SECTION, NULL, NULL, uci_type, CWMP_GET_FIRST_SECTION); section != NULL; section = cwmp_uci_walk_section(package, stype, NULL, NULL, CWMP_CMP_SECTION, NULL, section, uci_type, CWMP_GET_NEXT_SECTION))

#define cwmp_uci_foreach_varstate_sections(package, stype, section) \
	for (section = cwmp_uci_walk_section(package, stype, NULL, NULL, CWMP_CMP_SECTION, NULL, NULL, UCI_VARSTATE_CONFIG, CWMP_GET_FIRST_SECTION); section != NULL; section = cwmp_uci_walk_section(package, stype, NULL, NULL, CWMP_CMP_SECTION, NULL, section, UCI_VARSTATE_CONFIG, CWMP_GET_NEXT_SECTION))
#endif
