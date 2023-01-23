/*
 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	Copyright (C) 2013-2021 iopsys Software Solutions AB
 *	  Author Omar Kallel <omar.kallel@pivasoftware.com>
 */
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "cwmp_uci.h"
#include "log.h"

/*
 * UCI LOOKUP
 */
static inline bool cwmp_check_section_name(const char *str, bool name)
{
	if (!*str)
		return false;
	for (; *str; str++) {
		unsigned char c = *str;
		if (isalnum(c) || c == '_')
			continue;
		if (name || (c < 33) || (c > 126))
			return false;
	}
	return true;
}

int cwmp_uci_lookup_ptr(struct uci_context *ctx, struct uci_ptr *ptr, char *package, char *section, char *option, char *value)
{
	/*value*/
	ptr->value = value;
	/*package*/
	if (!package)
		return UCI_ERR_IO;
	ptr->package = package;

	/*section*/
	if (!section || !section[0]) {
		ptr->target = UCI_TYPE_PACKAGE;
		goto lookup;
	}
	ptr->section = section;
	if (ptr->section && !cwmp_check_section_name(ptr->section, true))
		ptr->flags |= UCI_LOOKUP_EXTENDED;

	/*option*/
	if (!option || !option[0]) {
		ptr->target = UCI_TYPE_SECTION;
		goto lookup;
	}
	ptr->target = UCI_TYPE_OPTION;
	ptr->option = option;

lookup:
	if (uci_lookup_ptr(ctx, ptr, NULL, true) != UCI_OK) {
		return UCI_ERR_PARSE;
	}
	return UCI_OK;
}

static int cwmp_uci_lookup_ptr_by_section(struct uci_context *ctx, struct uci_ptr *ptr, struct uci_section *s, char *option, char *value)
{
	if (s == NULL || s->package == NULL)
		return -1;

	/*value*/
	ptr->value = value;

	/*package*/
	ptr->package = s->package->e.name;
	ptr->p = s->package;

	/* section */
	ptr->section = s->e.name;
	ptr->s = s;

	/*option*/
	if (!option || !option[0]) {
		ptr->target = UCI_TYPE_SECTION;
		goto lookup;
	}
	ptr->target = UCI_TYPE_OPTION;
	ptr->option = option;
	ptr->flags |= UCI_LOOKUP_EXTENDED;
lookup:
	if (uci_lookup_ptr(ctx, ptr, NULL, false) != UCI_OK || !UCI_LOOKUP_COMPLETE)
		return -1;

	return 0;
}

/*
 * UCI INIT EXIT
 */

int cwmp_uci_standard_init(struct uci_paths *conf_path)
{
	if (conf_path == NULL)
		return -1;

	memset(conf_path, 0, sizeof(struct uci_paths));

	conf_path->uci_ctx = NULL;
	conf_path->conf_dir = "/etc/config";
	conf_path->save_dir = "/tmp/.uci";

	conf_path->uci_ctx = uci_alloc_context();
	if (conf_path->uci_ctx == NULL)
		return -1;

	uci_add_delta_path(conf_path->uci_ctx, conf_path->uci_ctx->savedir);
	uci_set_savedir(conf_path->uci_ctx, conf_path->save_dir);
	uci_set_confdir(conf_path->uci_ctx, conf_path->conf_dir);
	return 0;
}

int cwmp_uci_varstate_init(struct uci_paths *conf_path)
{
	if (conf_path == NULL)
		return -1;

	memset(conf_path, 0, sizeof(struct uci_paths));

	conf_path->uci_ctx = NULL;
	conf_path->conf_dir = "/var/state";
	conf_path->save_dir = NULL;

	conf_path->uci_ctx = uci_alloc_context();
	if (conf_path->uci_ctx == NULL)
		return -1;

	uci_add_delta_path(conf_path->uci_ctx, conf_path->uci_ctx->savedir);
	uci_set_savedir(conf_path->uci_ctx, conf_path->save_dir);
	uci_set_confdir(conf_path->uci_ctx, conf_path->conf_dir);
	return 0;
}

void cwmp_uci_exit(struct uci_paths *conf_path)
{
	if (conf_path == NULL)
		return;

	if (conf_path->uci_ctx == NULL)
		return;

	uci_free_context(conf_path->uci_ctx);
	conf_path->uci_ctx = NULL;
}

/*
 * UCI GET option value
 */
int cwmp_uci_get_option_value_string(char *package, char *section, char *option, struct uci_context *uci_ctx, char **value)
{
	struct uci_ptr ptr = { 0 };

	if (package == NULL || section == NULL || option == NULL || uci_ctx == NULL) {
		*value = NULL;
		return UCI_ERR_NOTFOUND;
	}

	if (cwmp_uci_lookup_ptr(uci_ctx, &ptr, package, section, option, NULL) != UCI_OK) {
		*value = NULL;
		return UCI_ERR_PARSE;
	}
	if (ptr.o && ptr.o->type == UCI_TYPE_LIST) {
		*value = cwmp_uci_list_to_string(&ptr.o->v.list, " ");
	} else if (ptr.o && ptr.o->v.string) {
		*value = icwmp_strdup(ptr.o->v.string);
	} else {
		*value = NULL;
		return UCI_ERR_NOTFOUND;
	}
	return UCI_OK;
}

int cwmp_uci_get_value_by_path(char *path, struct uci_context *uci_ctx, char **value)
{
	struct uci_ptr ptr;
	char *s;

	*value = NULL;

	if (path == NULL  || strlen(path) == 0) {
		CWMP_LOG(ERROR, "the entered path argument is empty");
		return UCI_ERR_IO;
	}
	if (uci_ctx == NULL) {
		*value = NULL;
		return UCI_ERR_NOTFOUND;
	}
	s = strdup(path);
	if (uci_lookup_ptr(uci_ctx, &ptr, s, true) != UCI_OK) {
		CWMP_LOG(ERROR, "Error occurred in uci get %s", path);
		free(s);
		return UCI_ERR_PARSE;
	}
	free(s);
	if (ptr.flags & UCI_LOOKUP_COMPLETE) {
		if (ptr.o == NULL || ptr.o->v.string == NULL) {
			CWMP_LOG(INFO, "%s not found or empty value", path);
			return UCI_OK;
		}
		*value = strdup(ptr.o->v.string);
	}
	return UCI_OK;
}

int uci_get_state_value(char *path, char **value)
{
	int error;
	struct uci_paths conf_path;

	if (path == NULL) {
		*value = NULL;
		return UCI_ERR_NOTFOUND;
	}

	if (cwmp_uci_varstate_init(&conf_path) != 0)
		return UCI_ERR_NOTFOUND;

	error = cwmp_uci_get_value_by_path(path, conf_path.uci_ctx, value);
	cwmp_uci_exit(&conf_path);

	return error;
}

int uci_get_value(char *path, char **value)
{
	int error;
	struct uci_paths conf_path;

	if (path == NULL) {
		*value = NULL;
		return UCI_ERR_NOTFOUND;
	}

	if (cwmp_uci_standard_init(&conf_path) != 0)
		return UCI_ERR_NOTFOUND;

	error = cwmp_uci_get_value_by_path(path, conf_path.uci_ctx, value);
	cwmp_uci_exit(&conf_path);

	return error;
}

int cwmp_uci_get_value_by_section_string(struct uci_section *s, char *option, char **value)
{
	struct uci_element *e;
	struct uci_option *o;

	*value = NULL;
	if (s == NULL || &s->options == NULL || option == NULL)
		goto not_found;

	uci_foreach_element(&s->options, e)
	{
		o = (uci_to_option(e));
		if (o && o->e.name && !strcmp(o->e.name, option)) {
			if (o->type == UCI_TYPE_LIST) {
				*value = cwmp_uci_list_to_string(&o->v.list, " ");
			} else {
				*value = o->v.string ? icwmp_strdup(o->v.string) : NULL;
			}
			return UCI_OK;
		}
	}

not_found:
	*value = NULL;
	return UCI_ERR_NOTFOUND;
}

int cwmp_uci_get_value_by_section_list(struct uci_section *s, char *option, struct uci_list **value)
{
	struct uci_element *e;
	struct uci_option *o;
	struct uci_list *list;
	char *pch = NULL, *spch = NULL;
	char dup[256];

	*value = NULL;

	if (s == NULL || option == NULL)
		return -1;

	uci_foreach_element(&s->options, e)
	{
		o = (uci_to_option(e));
		if (strcmp(o->e.name, option) == 0) {
			switch (o->type) {
			case UCI_TYPE_LIST:
				*value = &o->v.list;
				return 0;
			case UCI_TYPE_STRING:
				if (!o->v.string || (o->v.string)[0] == '\0')
					return 0;
				list = icwmp_calloc(1, sizeof(struct uci_list));
				cwmp_uci_list_init(list);
				snprintf(dup, sizeof(dup), "%s", o->v.string);
				pch = strtok_r(dup, " ", &spch);
				while (pch != NULL) {
					e = icwmp_calloc(1, sizeof(struct uci_element));
					e->name = pch;
					cwmp_uci_list_add(list, &e->list);
					pch = strtok_r(NULL, " ", &spch);
				}
				*value = list;
				return 0;
			default:
				return -1;
			}
		}
	}
	return -1;
}

/*
 * UCI Set option value
 */
int cwmp_uci_set_value_string(char *package, char *section, char *option, char *value, struct uci_context *uci_ctx)
{
	struct uci_ptr ptr = {0};

	if (package == NULL || section == NULL || option == NULL)
		return UCI_ERR_NOTFOUND;
	if (uci_ctx == NULL)
		return UCI_ERR_NOTFOUND;

	if (cwmp_uci_lookup_ptr(uci_ctx, &ptr, package, section, option, value))
		return UCI_ERR_PARSE;
	if (uci_set(uci_ctx, &ptr) != UCI_OK)
		return UCI_ERR_NOTFOUND;
	if (ptr.o)
		return UCI_OK;
	return UCI_ERR_NOTFOUND;
}

int cwmp_uci_set_value(char *package, char *section, char *option, char *value)
{
	struct uci_paths conf_path;

	if (cwmp_uci_standard_init(&conf_path) != 0)
		return UCI_ERR_NOTFOUND;

	int ret = cwmp_uci_set_value_string(package, section, option, value, conf_path.uci_ctx);
	cwmp_uci_exit(&conf_path);

	return ret;
}

int cwmp_uci_set_varstate_value(char *package, char*section, char *option, char *value)
{
	struct uci_paths conf_path;

	if (cwmp_uci_varstate_init(&conf_path) != 0)
		return UCI_ERR_NOTFOUND;

	int ret = cwmp_uci_set_value_string(package, section, option, value, conf_path.uci_ctx);
	cwmp_uci_exit(&conf_path);

	return ret;
}

int uci_set_value_by_path(char *path, char *value, struct uci_context *uci_ctx)
{
	struct uci_ptr ptr;
	int ret = UCI_OK;

	if (path == NULL || uci_ctx == NULL)
		return UCI_ERR_NOTFOUND;

	char cmd[256];
	snprintf(cmd, sizeof(cmd), "%s=%s", path, value);
	if (uci_lookup_ptr(uci_ctx, &ptr, cmd, true) != UCI_OK)
		return UCI_ERR_PARSE;

	ret = uci_set(uci_ctx, &ptr);

	if (ret == UCI_OK) {
		ret = uci_save(uci_ctx, ptr.p);
	} else {
		CWMP_LOG(ERROR, "UCI set not succeed %s", path);
		return UCI_ERR_NOTFOUND;
	}
	return ret;
}

int cwmp_uci_set_value_by_path(char *path, char *value)
{
	struct uci_paths conf_path;

	if (cwmp_uci_standard_init(&conf_path) != 0)
		return UCI_ERR_NOTFOUND;

	int ret = uci_set_value_by_path(path, value, conf_path.uci_ctx);
	cwmp_uci_exit(&conf_path);

	return ret;
}

int cwmp_uci_set_varstate_value_by_path(char *path, char *value)
{
	struct uci_paths conf_path;

	if (cwmp_uci_varstate_init(&conf_path) != 0)
		return UCI_ERR_NOTFOUND;

	int ret = uci_set_value_by_path(path, value, conf_path.uci_ctx);
	cwmp_uci_exit(&conf_path);

	return ret;
}

/*
 * UCI Lists
 */
void cwmp_uci_list_init(struct uci_list *ptr)
{
	ptr->prev = ptr;
	ptr->next = ptr;
}

void cwmp_uci_list_insert(struct uci_list *list, struct uci_list *ptr)
{
	list->next->prev = ptr;
	ptr->prev = list;
	ptr->next = list->next;
	list->next = ptr;
}

void cwmp_uci_list_add(struct uci_list *head, struct uci_list *ptr)
{
	cwmp_uci_list_insert(head->prev, ptr);
}

void cwmp_uci_list_del(struct uci_element *e)
{
	struct uci_list *ptr = e->list.prev;
	ptr->next = e->list.next;
}

void cwmp_delete_uci_element_from_list(struct uci_element *e)
{
	cwmp_uci_list_del(e);
	free(e->name);
	free(e);
}

void cwmp_free_uci_list(struct uci_list *list)
{
	struct uci_element *e = NULL, *tmp = NULL;
	if (list == NULL)
		return;

	// cppcheck-suppress unknownMacro
	uci_foreach_element_safe(list, e, tmp)
		cwmp_delete_uci_element_from_list(e);
}

char *cwmp_uci_list_to_string(struct uci_list *list, char *delimitor)
{
	if (delimitor == NULL)
		return NULL;
	if (list && !uci_list_empty(list)) {
		struct uci_element *e = NULL;
		char list_val[512] = { 0 };
		unsigned pos = 0;

		list_val[0] = 0;
		uci_foreach_element(list, e)
		{
			if (e->name)
				pos += snprintf(&list_val[pos], sizeof(list_val) - pos, "%s%s", e->name, delimitor);
		}

		if (pos)
			list_val[pos - 1] = 0;

		return icwmp_strdup(list_val);
	}
	return NULL;
}

int cwmp_uci_get_option_value_list(char *package, char *section, char *option, struct uci_context *uci_ctx, struct uci_list **value)
{
	struct uci_element *e = NULL;
	struct uci_ptr ptr = {0};
	struct uci_list *list;
	char *pch = NULL, *spch = NULL, *dup = NULL;
	int option_type;
	if (value == NULL)
		return UCI_ERR_IO;

	*value = NULL;

	if (package == NULL || section == NULL || option == NULL) {
		*value = NULL;
		return UCI_ERR_NOTFOUND;
	}
	if (cwmp_uci_lookup_ptr(uci_ctx, &ptr, package, section, option, NULL))
		return UCI_ERR_PARSE;

	if (ptr.o) {
		switch(ptr.o->type) {
			case UCI_TYPE_LIST:
				*value = &ptr.o->v.list;
				option_type = UCI_TYPE_LIST;
				break;
			case UCI_TYPE_STRING:
				if (!ptr.o->v.string || (ptr.o->v.string)[0] == '\0') {
					return UCI_TYPE_STRING;
				}
				list = calloc(1, sizeof(struct uci_list));
				cwmp_uci_list_init(list);
				dup = strdup((ptr.o && ptr.o->v.string) ? ptr.o->v.string : "");
				pch = strtok_r(dup, " ", &spch);
				while (pch != NULL) {
					e = calloc(1, sizeof(struct uci_element));
					if (e == NULL) {
						CWMP_LOG(ERROR, "uci %s: e is null", __FUNCTION__);
						return UCI_ERR_IO;
					}
					if (e->name)
						e->name = pch;
					cwmp_uci_list_add(list, &e->list);
					pch = strtok_r(NULL, " ", &spch);
				}
				*value = list;
				option_type = UCI_TYPE_STRING;
				break;
			default:
				return UCI_ERR_NOTFOUND;
		}
	} else {
		return UCI_ERR_NOTFOUND;
	}
	return option_type;
}

int cwmp_uci_get_cwmp_standard_option_value_list(char *package, char *section, char *option, struct uci_list **value)
{
	struct uci_paths conf_path;

	if (cwmp_uci_standard_init(&conf_path) != 0)
		return UCI_ERR_NOTFOUND;

	int ret = cwmp_uci_get_option_value_list(package, section, option, conf_path.uci_ctx, value);
	cwmp_uci_exit(&conf_path);

	return ret;
}

int cwmp_uci_get_cwmp_varstate_option_value_list(char *package, char *section, char *option, struct uci_list **value)
{
	struct uci_paths conf_path;

	if (cwmp_uci_varstate_init(&conf_path) != 0)
		return UCI_ERR_NOTFOUND;

	int ret = cwmp_uci_get_option_value_list(package, section, option, conf_path.uci_ctx, value);
	cwmp_uci_exit(&conf_path);

	return ret;
}

int cwmp_uci_add_list_value(char *package, char *section, char *option, char *value, uci_config_paths uci_type)
{
	struct uci_ptr ptr = {0};
	int error = UCI_OK;
	struct uci_paths conf_path;
	int ret = -1;

	if (uci_type == UCI_STANDARD_CONFIG) {
		ret = cwmp_uci_standard_init(&conf_path);
	} else if (uci_type == UCI_VARSTATE_CONFIG) {
		ret = cwmp_uci_varstate_init(&conf_path);	
	}

	if (ret != 0)
		return UCI_ERR_NOTFOUND;

	if (package == NULL || section == NULL || option == NULL) {
		cwmp_uci_exit(&conf_path);
		return UCI_ERR_NOTFOUND;
	}

	if (cwmp_uci_lookup_ptr(conf_path.uci_ctx, &ptr, package, section, option, value)) {
		cwmp_uci_exit(&conf_path);
		return UCI_ERR_PARSE;
	}

	error = uci_add_list(conf_path.uci_ctx, &ptr);
	if (error != UCI_OK) {
		cwmp_uci_exit(&conf_path);
		return error;
	}

	cwmp_uci_exit(&conf_path);
	return UCI_OK;
}

int cwmp_uci_del_list_value(char *package, char *section, char *option, char *value, uci_config_paths uci_type)
{
	struct uci_ptr ptr = {0};
	struct uci_paths conf_path;
	int ret = -1;

	if (uci_type == UCI_STANDARD_CONFIG) {
		ret = cwmp_uci_standard_init(&conf_path);
	} else if (uci_type == UCI_VARSTATE_CONFIG) {
		ret = cwmp_uci_varstate_init(&conf_path);	
	}

	if (ret != 0)
		return UCI_ERR_NOTFOUND;

	if (package == NULL || section == NULL || option == NULL) {
		cwmp_uci_exit(&conf_path);
		return UCI_ERR_NOTFOUND;
	}

	if (cwmp_uci_lookup_ptr(conf_path.uci_ctx, &ptr, package, section, option, value)) {
		cwmp_uci_exit(&conf_path);
		return -1;
	}

	if (uci_del_list(conf_path.uci_ctx, &ptr) != UCI_OK) {
		cwmp_uci_exit(&conf_path);
		return -1;
	}

	cwmp_uci_exit(&conf_path);
	return 0;
}

int uci_add_list_value(char *cmd, uci_config_paths uci_type)
{
	struct uci_ptr ptr;
	int error = UCI_OK;
	struct uci_paths conf_path;
	int ret = -1;

	if (cmd == NULL)
		return UCI_ERR_NOTFOUND;

	if (uci_type == UCI_STANDARD_CONFIG) {
		ret = cwmp_uci_standard_init(&conf_path);
	} else if (uci_type == UCI_VARSTATE_CONFIG) {
		ret = cwmp_uci_varstate_init(&conf_path);	
	}

	if (ret != 0)
		return UCI_ERR_NOTFOUND;

	if (uci_lookup_ptr(conf_path.uci_ctx, &ptr, cmd, true) != UCI_OK) {
		cwmp_uci_exit(&conf_path);
		return UCI_ERR_PARSE;
	}

	error = uci_add_list(conf_path.uci_ctx, &ptr);

	if (error == UCI_OK) {
		error = uci_save(conf_path.uci_ctx, ptr.p);
	} else {
		CWMP_LOG(ERROR, "UCI delete not succeed %s", cmd);
		cwmp_uci_exit(&conf_path);
		return UCI_ERR_NOTFOUND;
	}

	cwmp_uci_exit(&conf_path);
	return error;
}

/*
 * UCI ADD Section
 */

int cwmp_uci_add_section(char *package, char *stype, uci_config_paths uci_type , struct uci_section **s)
{
	struct uci_ptr ptr = {0};
	char fname[128];
	struct uci_paths conf_path;
	int ret = -1;

	*s = NULL;

	if (package == NULL || stype == NULL)
		return UCI_ERR_NOTFOUND;

	if (uci_type == UCI_STANDARD_CONFIG) {
		ret = cwmp_uci_standard_init(&conf_path);
	} else if (uci_type == UCI_VARSTATE_CONFIG) {
		ret = cwmp_uci_varstate_init(&conf_path);	
	}

	if (ret != 0)
		return UCI_ERR_NOTFOUND;

	snprintf(fname, sizeof(fname), "%s/%s", conf_path.conf_dir, package);

	if (!file_exists(fname)) {
		FILE *fptr = fopen(fname, "w");
		if (fptr) {
			fclose(fptr);
		} else {
			cwmp_uci_exit(&conf_path);
			return UCI_ERR_UNKNOWN;
		}
	}

	if (cwmp_uci_lookup_ptr(conf_path.uci_ctx, &ptr, package, NULL, NULL, NULL) == 0
		&& uci_add_section(conf_path.uci_ctx, ptr.p, stype, s) == UCI_OK) {
		CWMP_LOG(INFO, "New uci section %s added successfully", stype);
	} else {
		cwmp_uci_exit(&conf_path);
		return UCI_ERR_NOTFOUND;
	}

	cwmp_uci_exit(&conf_path);
	return UCI_OK;
}

struct uci_section* get_section_by_section_name(char *package, char *stype, char* sname, uci_config_paths uci_type)
{
	struct uci_section *s;

	if (package == NULL || stype == NULL || sname == NULL)
		return NULL;

	struct uci_paths conf_path;
	int ret = 0;
	if (uci_type == UCI_STANDARD_CONFIG) {
		ret = cwmp_uci_standard_init(&conf_path);
	} else {
		ret = cwmp_uci_varstate_init(&conf_path);
	}

	if (ret != 0)
		return NULL;

	cwmp_uci_foreach_sections(package, stype, conf_path.uci_ctx, s) {
		if (strcmp(section_name(s), sname) == 0) {
			cwmp_uci_exit(&conf_path);
			return s;
		}
	}

 	return NULL;

}

int cwmp_uci_rename_section_by_section(struct uci_section *s, char *value, uci_config_paths uci_type)
{
	struct uci_ptr up = {0};
	struct uci_paths conf_path;
	int ret = -1;

	if (s == NULL)
		return UCI_ERR_NOTFOUND;

	if (uci_type == UCI_STANDARD_CONFIG) {
		ret = cwmp_uci_standard_init(&conf_path);
	} else if (uci_type == UCI_VARSTATE_CONFIG) {
		ret = cwmp_uci_varstate_init(&conf_path);	
	}

	if (ret != 0)
		return UCI_ERR_NOTFOUND;

	if (cwmp_uci_lookup_ptr_by_section(conf_path.uci_ctx, &up, s, NULL, value) == -1) {
		cwmp_uci_exit(&conf_path);
		return UCI_ERR_PARSE;
	}

	if (uci_rename(conf_path.uci_ctx, &up) != UCI_OK) {
		cwmp_uci_exit(&conf_path);
		return UCI_ERR_NOTFOUND;
	}

	cwmp_uci_exit(&conf_path);
	return UCI_OK;
}

int cwmp_uci_add_section_with_specific_name(char *package, char *stype, char *section_name, uci_config_paths uci_type)
{
	struct uci_section *s = NULL;

	if (package == NULL || stype == NULL || section_name == NULL)
		return UCI_ERR_NOTFOUND;
	if (get_section_by_section_name(package, stype, section_name, uci_type) != NULL)
		return UCI_ERR_DUPLICATE;
	if (cwmp_uci_add_section(package, stype, uci_type, &s) != UCI_OK)
		return UCI_ERR_UNKNOWN;

	return cwmp_uci_rename_section_by_section(s, section_name, uci_type);
}

/*
 * UCI Delete Value
 */
int uci_delete_value(char *path, int uci_type)
{
	struct uci_ptr ptr;
	struct uci_paths conf_path;
	int ret = -1;

	if (path == NULL)
		return UCI_ERR_NOTFOUND;

	if (uci_type == UCI_STANDARD_CONFIG) {
		ret = cwmp_uci_standard_init(&conf_path);
	} else if (uci_type == UCI_VARSTATE_CONFIG) {
		ret = cwmp_uci_varstate_init(&conf_path);	
	}

	if (ret != 0)
		return UCI_ERR_NOTFOUND;

	if (uci_lookup_ptr(conf_path.uci_ctx, &ptr, path, true) != UCI_OK) {
		cwmp_uci_exit(&conf_path);
		return CWMP_GEN_ERR;
	}

	int error = uci_delete(conf_path.uci_ctx, &ptr);

	if (error == UCI_OK) {
		error = uci_save(conf_path.uci_ctx, ptr.p);
	} else {
		CWMP_LOG(ERROR, "UCI delete not succeed %s", path);
		cwmp_uci_exit(&conf_path);
		return CWMP_GEN_ERR;
	}

	cwmp_uci_exit(&conf_path);
	return error;
}

int cwmp_uci_get_section_type(char *package, char *section, uci_config_paths uci_type, char **value)
{
	struct uci_ptr ptr = {0};
	struct uci_paths conf_path;
	int ret = -1;

	if (package == NULL || section == NULL) {
		*value = NULL;
		return UCI_ERR_NOTFOUND;
	}

	if (uci_type == UCI_STANDARD_CONFIG) {
		ret = cwmp_uci_standard_init(&conf_path);
	} else if (uci_type == UCI_VARSTATE_CONFIG) {
		ret = cwmp_uci_varstate_init(&conf_path);	
	}

	if (ret != 0)
		return UCI_ERR_NOTFOUND;

	if (cwmp_uci_lookup_ptr(conf_path.uci_ctx, &ptr, package, section, NULL, NULL)) {
		*value = "";
		cwmp_uci_exit(&conf_path);
		return -1;
	}

	if (ptr.s) {
		*value = icwmp_strdup(ptr.s->type ? ptr.s->type : "");
	} else {
		*value = "";
	}

	cwmp_uci_exit(&conf_path);
	return UCI_OK;
}

struct uci_section *cwmp_uci_walk_section(char *package, char *stype, void *arg1, void *arg2, int cmp, int (*filter)(struct uci_section *s, void *value), struct uci_section *prev_section, struct uci_context *uci_ctx, int walk)
{
	struct uci_section *s = NULL;
	struct uci_element *e, *m;
	char *value = NULL, *pch = NULL, *spch = NULL;
	char dup[256];
	struct uci_list *list_value, *list_section;
	struct uci_ptr ptr = { 0 };

	if (walk == CWMP_GET_FIRST_SECTION) {
		if (cwmp_uci_lookup_ptr(uci_ctx, &ptr, package, NULL, NULL, NULL) != UCI_OK)
			goto end;

		list_section = &(ptr.p)->sections;
		e = list_to_element(list_section->next);
	} else {
		list_section = &prev_section->package->sections;
		e = list_to_element(prev_section->e.list.next);
	}

	while (&e->list != list_section) {
		s = uci_to_section(e);
		if (s && s->type && stype && strcmp(s->type, stype) == 0) {
			switch (cmp) {
			case CWMP_CMP_SECTION:
				goto end;
			case CWMP_CMP_OPTION_EQUAL:
				if (arg1 == NULL || arg2 == NULL)
					break;
				cwmp_uci_get_value_by_section_string(s, (char *)arg1, &value);
				if (strcmp(value, (char *)arg2) == 0)
					goto end;
				break;
			case CWMP_CMP_OPTION_CONTAINING:
				if (arg1 == NULL || arg2 == NULL)
					break;
				cwmp_uci_get_value_by_section_string(s, (char *)arg1, &value);
				if (strstr(value, (char *)arg2))
					goto end;
				break;
			case CWMP_CMP_OPTION_CONT_WORD:
				cwmp_uci_get_value_by_section_string(s, (char *)arg1, &value);
				snprintf(dup, sizeof(dup), "%s", value);
				pch = strtok_r(dup, " ", &spch);
				while (pch != NULL) {
					if (strcmp((char *)arg2, pch) == 0)
						goto end;

					pch = strtok_r(NULL, " ", &spch);
				}
				break;
			case CWMP_CMP_LIST_CONTAINING:
				cwmp_uci_get_value_by_section_list(s, (char *)arg1, &list_value);
				if (list_value != NULL) {
					uci_foreach_element(list_value, m)
					{
						if (strcmp(m->name, (char *)arg2) == 0)
							goto end;
					}
				}
				break;
			case CWMP_CMP_FILTER_FUNC:
				if (filter(s, arg1) == 0)
					goto end;
				break;
			default:
				break;
			}
		}
		e = list_to_element(e->list.next);
		s = NULL;
	}
end:
	return s;
}

int cwmp_commit_package(char *package, uci_config_paths uci_type)
{
	struct uci_ptr ptr = { 0 };
	struct uci_paths conf_path;
	int ret = -1;

	if (uci_type == UCI_STANDARD_CONFIG) {
		ret = cwmp_uci_standard_init(&conf_path);
	} else if (uci_type == UCI_VARSTATE_CONFIG) {
		ret = cwmp_uci_varstate_init(&conf_path);	
	}

	if (ret != 0)
		return UCI_ERR_NOTFOUND;

	if (uci_lookup_ptr(conf_path.uci_ctx, &ptr, package, true) != UCI_OK) {
		cwmp_uci_exit(&conf_path);
		return -1;
	}

	if (uci_commit(conf_path.uci_ctx, &ptr.p, false) != UCI_OK) {
		cwmp_uci_exit(&conf_path);
		return -1;
	}

	cwmp_uci_exit(&conf_path);
	return 0;
}

int cwmp_uci_import(char *package_name, const char *input_path, uci_config_paths uci_type)
{
	struct uci_package *package = NULL;
	struct uci_element *e = NULL;
	int ret = CWMP_OK, err = -1;
	FILE *input = fopen(input_path, "r");
	if (!input)
		return -1;

	struct uci_paths conf_path;

	if (uci_type == UCI_STANDARD_CONFIG) {
		err = cwmp_uci_standard_init(&conf_path);
	} else if (uci_type == UCI_VARSTATE_CONFIG) {
		err = cwmp_uci_varstate_init(&conf_path);	
	}

	if (err != 0) {
		fclose(input);
		return err;
	}

	if (uci_import(conf_path.uci_ctx, input, package_name, &package, (package_name != NULL)) != UCI_OK) {
		ret = -1;
		goto end;
	}

	if (conf_path.uci_ctx == NULL) {
		ret = -1;
		goto end;
	}

	uci_foreach_element(&conf_path.uci_ctx->root, e)
	{
		struct uci_package *p = uci_to_package(e);
		if (uci_commit(conf_path.uci_ctx, &p, true) != UCI_OK)
			ret = CWMP_GEN_ERR;
	}

end:
	fclose(input);
	cwmp_uci_exit(&conf_path);
	return ret;
}

int cwmp_uci_export_package(char *package, const char *output_path, uci_config_paths uci_type)
{
	struct uci_ptr ptr = { 0 };
	int ret = 0;
	FILE *out = fopen(output_path, "a");
	if (!out)
		return -1;

	struct uci_paths conf_path;

	if (uci_type == UCI_STANDARD_CONFIG) {
		ret = cwmp_uci_standard_init(&conf_path);
	} else if (uci_type == UCI_VARSTATE_CONFIG) {
		ret = cwmp_uci_varstate_init(&conf_path);	
	}

	if (ret != 0) {
		fclose(out);
		return ret;
	}

	if (uci_lookup_ptr(conf_path.uci_ctx, &ptr, package, true) != UCI_OK) {
		ret = -1;
		goto end;
	}

	if (uci_export(conf_path.uci_ctx, out, ptr.p, true) != UCI_OK)
		ret = -1;

end:
	fclose(out);
	cwmp_uci_exit(&conf_path);
	return ret;
}

int cwmp_uci_export(const char *output_path, uci_config_paths uci_type)
{
	char **configs = NULL;
	char **p;
	struct uci_paths conf_path;
	int ret = 0;

	if (uci_type == UCI_STANDARD_CONFIG) {
		ret = cwmp_uci_standard_init(&conf_path);
	} else if (uci_type == UCI_VARSTATE_CONFIG) {
		ret = cwmp_uci_varstate_init(&conf_path);	
	}

	if (ret != 0) {
		return -1;
	}

	if (uci_list_configs(conf_path.uci_ctx, &configs) != UCI_OK) {
		cwmp_uci_exit(&conf_path);
		return -1;
	}

	cwmp_uci_exit(&conf_path);
	if (configs == NULL)
		return -1;

	for (p = configs; *p; p++)
		cwmp_uci_export_package(*p, output_path, uci_type);

	FREE(configs);
	return 0;
}
