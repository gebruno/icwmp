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

#define UCI_CONFIG_DIR "/etc/config/"
#define ETC_DB_CONFIG "/etc/board-db/config"
#define VARSTATE_CONFIG "/var/state"
#define DHCP_OPTION_READ_MAX_RETRY 5
#define UCI_OPTION_READ_INTERVAL 5

#define section_name(s) s ? (s)->e.name : ""

typedef enum uci_config_paths
{
	UCI_STANDARD_CONFIG,
	UCI_VARSTATE_CONFIG,
	UCI_ETCICWMPD_CONFIG,
} uci_config_paths;

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

int cwmp_uci_init();
void cwmp_uci_exit(void);
void cwmp_uci_reinit(void);

int cwmp_uci_get_option_value_list(char *package, char *section, char *option, uci_config_paths uci_type, struct uci_list **value);

int uci_get_state_value(char *cmd, char **value);

int uci_set_value_by_path(char *cmd, char *value, uci_config_paths uci_type);

int uci_get_value(char *cmd, char **value);

struct uci_section *cwmp_uci_walk_section(char *package, char *stype, struct uci_section *prev_section, uci_config_paths uci_type, int walk);

int cwmp_uci_get_value_by_section_string(struct uci_section *s, char *option, char **value);

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

#define cwmp_uci_foreach_sections(package, stype, uci_type, section) \
	for (section = cwmp_uci_walk_section(package, stype, NULL, uci_type, CWMP_GET_FIRST_SECTION); \
		section != NULL; \
		section = cwmp_uci_walk_section(package, stype, section, uci_type, CWMP_GET_NEXT_SECTION))

#endif
