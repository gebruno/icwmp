/*
 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	Copyright (C) 2013-2021 iopsys Software Solutions AB
 *	  Author Omar Kallel <omar.kallel@pivasoftware.com>
 */

#include <libubox/blobmsg_json.h>
#include <stdlib.h>
#include <regex.h>

#include "cwmp_du_state.h"
#include "ubus.h"
#include "log.h"
#include "datamodel_interface.h"
#include "cwmp_time.h"
#include "backupSession.h"
#include "event.h"

LIST_HEAD(list_change_du_state);
pthread_mutex_t mutex_change_du_state = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t threshold_change_du_state;

void ubus_du_state_callback(struct ubus_request *req, int type __attribute__((unused)), struct blob_attr *msg)
{
	char **fault = (char **)req->priv;
	const struct blobmsg_policy p[1] = { { "status", BLOBMSG_TYPE_BOOL } };
	struct blob_attr *tb[1] = { NULL };
	blobmsg_parse(p, 1, tb, blobmsg_data(msg), blobmsg_len(msg));
	if (tb[0] && blobmsg_get_bool(tb[0])) {
		*fault = NULL;
	} else {
		*fault = strdup("9010");
	}
}

int cwmp_du_install(char *url, char *uuid, char *user, char *pass, char *env_name, int env_id, char **fault_code)
{
	int e;
	e = cwmp_ubus_call("swmodules", "du_install",
			   CWMP_UBUS_ARGS{ { "ee_name", {.str_val = env_name }, UBUS_String }, { "eeid", {.int_val = env_id }, UBUS_Integer }, { "url", {.str_val = url }, UBUS_String }, { "uuid", {.str_val = uuid ? uuid : "" }, UBUS_String }, { "username", {.str_val = user ? user : "" }, UBUS_String }, { "password", {.str_val = pass ? pass : ""}, UBUS_String } }, 6,
			   ubus_du_state_callback, fault_code);
	if (e < 0) {
		CWMP_LOG(INFO, "Change du state install failed: Ubus err code: %d", e);
		return FAULT_CPE_INTERNAL_ERROR;
	}
	return FAULT_CPE_NO_FAULT;
}

int cwmp_du_update(char *url, char *uuid, char *user, char *pass, char *env_name, int env_id, char **fault_code)
{
	int e;
	e = cwmp_ubus_call("swmodules", "du_update", CWMP_UBUS_ARGS{ { "ee_name", {.str_val = env_name }, UBUS_String }, { "eeid", {.int_val = env_id }, UBUS_Integer }, { "url", {.str_val = url }, UBUS_String }, { "uuid", {.str_val = uuid ? uuid : "" }, UBUS_String }, { "username", {.str_val = user ? user : "" }, UBUS_String }, { "password", {.str_val = pass ? pass : "" }, UBUS_String } }, 6, ubus_du_state_callback,
			   fault_code);
	if (e < 0) {
		CWMP_LOG(INFO, "Change du state update failed: Ubus err code: %d", e);
		return FAULT_CPE_INTERNAL_ERROR;
	}
	return FAULT_CPE_NO_FAULT;
}

int cwmp_du_uninstall(char *package_name, char *env_name, int env_id, char **fault_code)
{
	int e;
	e = cwmp_ubus_call("swmodules", "du_uninstall", CWMP_UBUS_ARGS{ { "ee_name", {.str_val = env_name }, UBUS_String }, { "eeid", {.int_val = env_id }, UBUS_Integer }, { "du_name", {.str_val = package_name }, UBUS_String } }, 3, ubus_du_state_callback, fault_code);
	if (e < 0) {
		CWMP_LOG(INFO, "Change du state uninstall failed: Ubus err code: %d", e);
		return FAULT_CPE_INTERNAL_ERROR;
	}
	return FAULT_CPE_NO_FAULT;
}


static char *get_software_module_object_eq(char *param1, char *val1, char *param2, char *val2, struct list_head *sw_parameters)
{
	char *err = NULL;

	err = cwmp_get_parameter_values("Device.SoftwareModules.DeploymentUnit.", sw_parameters);
	if (err)
		return NULL;

	struct cwmp_dm_parameter *param_value;
	char instance[8] = {0};
	regex_t regex1 = {};
	regex_t regex2 = {};
	bool softwaremodule_filter_param = false;
	char regstr1[256];
	snprintf(regstr1, sizeof(regstr1), "^Device.SoftwareModules.DeploymentUnit.[0-9][0-9]*.%s$",param1);
	regcomp(&regex1, regstr1, 0);
	if (param2) {
		char regstr2[256];
		snprintf(regstr2, sizeof(regstr2), "^Device.SoftwareModules.DeploymentUnit.[0-9][0-9]*.%s$",param2);
		regcomp(&regex2, regstr2, 0);
	}

	list_for_each_entry (param_value, sw_parameters, list) {
		if (regexec(&regex1, param_value->name, 0, NULL, 0) == 0 && strcmp(param_value->value, val1) == 0)
			softwaremodule_filter_param = true;

		if (param2 && regexec(&regex2, param_value->name, 0, NULL, 0) == 0 && strcmp(param_value->value, val2) == 0)
			softwaremodule_filter_param = true;

		if (softwaremodule_filter_param == false)
			continue;

		snprintf(instance, (size_t)(strchr(param_value->name + strlen("Device.SoftwareModules.DeploymentUnit."), '.') - param_value->name - strlen("Device.SoftwareModules.DeploymentUnit.") + 1), "%s", (char *)(param_value->name + strlen("Device.SoftwareModules.DeploymentUnit.")));
		break;
	}
	return (strlen(instance) > 0) ? strdup(instance) : NULL;
}

static int get_deployment_unit_name_version(char *uuid, char **name, char **version, char **env)
{
	char *sw_by_uuid_instance = NULL, name_param[128], version_param[128], environment_param[128];
	LIST_HEAD(sw_parameters);
	sw_by_uuid_instance = get_software_module_object_eq("UUID", uuid, NULL, NULL, &sw_parameters);
	if (!sw_by_uuid_instance)
		return 0;

	snprintf(name_param, sizeof(name_param), "Device.SoftwareModules.DeploymentUnit.%s.Name", sw_by_uuid_instance);
	snprintf(version_param, sizeof(version_param), "Device.SoftwareModules.DeploymentUnit.%s.Version", sw_by_uuid_instance);
	snprintf(environment_param, sizeof(environment_param), "Device.SoftwareModules.DeploymentUnit.%s.ExecutionEnvRef", sw_by_uuid_instance);
	struct cwmp_dm_parameter *param_value;
	list_for_each_entry (param_value, &sw_parameters, list) {
		if (strcmp(param_value->name, name_param) == 0) {
			*name = strdup(param_value->value);
			continue;
		}
		if (strcmp(param_value->name, version_param) == 0) {
			*version = strdup(param_value->value);
			continue;
		}
		if (strcmp(param_value->name, environment_param) == 0) {
			*env = strdup(param_value->value);
			continue;
		}
	}
	cwmp_free_all_dm_parameter_list(&sw_parameters);
	return 1;
}

char *get_deployment_unit_by_uuid(char *uuid)
{
	if (uuid == NULL)
		return NULL;
	char *sw_by_uuid_instance = NULL;
	LIST_HEAD(sw_parameters);
	sw_by_uuid_instance = get_software_module_object_eq("UUID", uuid, NULL, NULL, &sw_parameters);
	return sw_by_uuid_instance;
}

static char *get_deployment_unit_reference(char *package_name, char *package_env)
{
	LIST_HEAD(sw_parameters);
	char *sw_by_name_env_instance = NULL, *deployment_unit_ref = NULL;
	sw_by_name_env_instance = get_software_module_object_eq("Name", package_name, "ExecutionEnvRef", package_env, &sw_parameters);
	cwmp_free_all_dm_parameter_list(&sw_parameters);
	if (!sw_by_name_env_instance)
		return NULL;

	cwmp_asprintf(&deployment_unit_ref, "Device.SoftwareModules.DeploymentUnit.%s", sw_by_name_env_instance);
	return deployment_unit_ref;
}

static bool environment_exists(char *environment_path)
{
	LIST_HEAD(environment_list);
	char *err = cwmp_get_parameter_values(environment_path, &environment_list);
	cwmp_free_all_dm_parameter_list(&environment_list);
	if (err)
		return false;
	else
		return true;
}

static char *get_exec_env_name(char *environment_path)
{
	char env_param[256], *env_name = "";

	LIST_HEAD(environment_list);
	char *err = cwmp_get_parameter_values(environment_path, &environment_list);
	if (err)
		return strdup("");

	struct cwmp_dm_parameter *param_value;
	snprintf(env_param, sizeof(env_param), "%sName", environment_path);
	list_for_each_entry (param_value, &environment_list, list) {
		if (strcmp(param_value->name, env_param) == 0) {
			env_name = strdup(param_value->value);
			break;
		}
	}
	cwmp_free_all_dm_parameter_list(&environment_list);
	return env_name;
}

static int cwmp_launch_du_install(char *url, char *uuid, char *user, char *pass, char *env_name, int env_id, struct opresult **pchange_du_state_complete)
{
	int error = FAULT_CPE_NO_FAULT;
	char *fault_code;

	(*pchange_du_state_complete)->start_time = strdup(mix_get_time());
	cwmp_du_install(url, uuid, user, pass, env_name, env_id, &fault_code);

	if (fault_code != NULL) {
		if (fault_code[0] == '9') {
			int i;
			for (i = 1; i < __FAULT_CPE_MAX; i++) {
				if (strcmp(FAULT_CPE_ARRAY[i].CODE, fault_code) == 0) {
					error = i;
					break;
				}
			}
		}
		free(fault_code);
	}
	return error;
}

static int cwmp_launch_du_update(char *uuid, char *url, char *user, char *pass, char *env_name, int env_id, struct opresult **pchange_du_state_complete)
{
	int error = FAULT_CPE_NO_FAULT;
	char *fault_code;

	(*pchange_du_state_complete)->start_time = strdup(mix_get_time());

	cwmp_du_update(url, uuid, user, pass, env_name, env_id, &fault_code);
	if (fault_code != NULL) {
		if (fault_code[0] == '9') {
			int i;
			for (i = 1; i < __FAULT_CPE_MAX; i++) {
				if (strcmp(FAULT_CPE_ARRAY[i].CODE, fault_code) == 0) {
					error = i;
					break;
				}
			}
		}
		free(fault_code);
	}
	return error;
}

static int cwmp_launch_du_uninstall(char *package_name, char *env_name, int env_id, struct opresult **pchange_du_state_complete)
{
	int error = FAULT_CPE_NO_FAULT;
	char *fault_code;

	(*pchange_du_state_complete)->start_time = strdup(mix_get_time());

	cwmp_du_uninstall(package_name, env_name, env_id, &fault_code);

	if (fault_code != NULL) {
		if (fault_code[0] == '9') {
			int i;
			for (i = 1; i < __FAULT_CPE_MAX; i++) {
				if (strcmp(FAULT_CPE_ARRAY[i].CODE, fault_code) == 0) {
					error = i;
					break;
				}
			}
		}
		free(fault_code);
	}
	return error;
}

int get_exec_env_id(char *exec_env_ref)
{
	int id = 0;
	sscanf(exec_env_ref, "Device.SoftwareModules.ExecEnv.%d", &id);
	return id;
}

char *get_package_name_by_url(char *url)
{
        char *slash = strrchr(url, '/');
        if (slash == NULL)
                return NULL;
        return slash+1;
}

int get_du_version(char *du_ref, char **version)
{
	char version_param_path[126];
	snprintf(version_param_path, sizeof(version_param_path), "%s.Version", du_ref);
	return cwmp_get_leaf_value(version_param_path, version);
}

void *thread_cwmp_rpc_cpe_change_du_state(void *v)
{
	struct cwmp *cwmp = (struct cwmp *)v;
	struct timespec change_du_state_timeout = { 50, 0 };
	int error = FAULT_CPE_NO_FAULT;
	struct du_state_change_complete *pdu_state_change_complete;
	long int time_of_grace = 216000;
	char *package_version = NULL;
	char *package_name = NULL;
	char *package_env = NULL;
	struct operations *p, *q;
	struct opresult *res;
	char *du_ref = NULL;

	for (;;) {

		if (thread_end)
			break;
		if (list_change_du_state.next != &(list_change_du_state)) {
			struct change_du_state *pchange_du_state = list_entry(list_change_du_state.next, struct change_du_state, list);
			time_t current_time = time(NULL);
			time_t timeout = current_time - pchange_du_state->timeout;

			if ((timeout >= 0) && (timeout > time_of_grace)) {
				pthread_mutex_lock(&mutex_change_du_state);
				pdu_state_change_complete = calloc(1, sizeof(struct du_state_change_complete));
				if (pdu_state_change_complete != NULL) {
					error = FAULT_CPE_DOWNLOAD_FAILURE;
					INIT_LIST_HEAD(&(pdu_state_change_complete->list_opresult));
					pdu_state_change_complete->command_key = strdup(pchange_du_state->command_key ? pchange_du_state->command_key : "");
					pdu_state_change_complete->timeout = pchange_du_state->timeout;
					list_for_each_entry_safe (p, q, &pchange_du_state->list_operation, list) {
						res = calloc(1, sizeof(struct opresult));
						list_add_tail(&(res->list), &(pdu_state_change_complete->list_opresult));
						res->uuid = strdup(p->uuid);
						res->version = strdup(p->version);
						res->current_state = strdup("Failed");
						res->start_time = strdup(mix_get_time());
						res->complete_time = strdup(res->start_time);
						res->fault = error;
					}
					bkp_session_insert_du_state_change_complete(pdu_state_change_complete);
					bkp_session_save();
					cwmp_root_cause_changedustate_complete(cwmp, pdu_state_change_complete);
				}
				list_del(&(pchange_du_state->list));
				cwmp_free_change_du_state_request(pchange_du_state);
				pthread_mutex_unlock(&mutex_change_du_state);
				continue;
			}

			if ((timeout >= 0) && (timeout <= time_of_grace)) {
				pthread_mutex_lock(&(cwmp->mutex_session_send));
				pdu_state_change_complete = calloc(1, sizeof(struct du_state_change_complete));
				if (pdu_state_change_complete != NULL) {
					error = FAULT_CPE_NO_FAULT;
					INIT_LIST_HEAD(&(pdu_state_change_complete->list_opresult));
					pdu_state_change_complete->command_key = strdup(pchange_du_state->command_key);
					pdu_state_change_complete->timeout = pchange_du_state->timeout;

					list_for_each_entry_safe (p, q, &pchange_du_state->list_operation, list) {
						res = calloc(1, sizeof(struct opresult));
						list_add_tail(&(res->list), &(pdu_state_change_complete->list_opresult));
						switch (p->type) {
						case DU_INSTALL:
							if (!environment_exists(p->executionenvref)) {
								res->fault = FAULT_CPE_INTERNAL_ERROR;
								break;
							}

							error = cwmp_launch_du_install(p->url, p->uuid, p->username, p->password, get_exec_env_name(p->executionenvref), get_exec_env_id(p->executionenvref), &res);

							package_name = get_package_name_by_url(p->url);

							if (error == FAULT_CPE_NO_FAULT) {
								du_ref = (package_name && p->executionenvref) ? get_deployment_unit_reference(package_name, p->executionenvref) : NULL;
								get_du_version(du_ref, &package_version);
								res->du_ref = strdup("");
								res->uuid = strdup("");
								res->current_state = strdup("Installed");
								res->resolved = 1;
								res->version = strdup("");
								FREE(du_ref);
							} else {
								res->uuid = strdup(p->uuid ? p->uuid : "");
								res->current_state = strdup("Failed");
								res->resolved = 0;
							}

							res->complete_time = strdup(mix_get_time());
							res->fault = error;
							break;

						case DU_UPDATE:
							if (p->url == NULL || p->uuid == NULL || *(p->url) == '\0' || *(p->uuid) == '\0') {
								error = FAULT_CPE_UNKNOWN_DEPLOYMENT_UNIT;
								break;
							}

							du_ref = get_deployment_unit_by_uuid(p->uuid);
							if (du_ref == NULL) {
								error = FAULT_CPE_UNKNOWN_DEPLOYMENT_UNIT;
								break;
							}
							char *execenv = calloc(40, sizeof(char));

							snprintf(execenv, 40, "Device.SoftwareModules.ExecEnv.%s.", du_ref);
							error = cwmp_launch_du_update(p->uuid, p->url, p->username, p->password, get_exec_env_name(execenv), get_exec_env_id(execenv), &res);

							res->uuid = strdup(p->uuid ? p->uuid : "");

							if (error == FAULT_CPE_NO_FAULT) {
								res->current_state = strdup("Installed");
								res->resolved = 1;
							} else {
								res->current_state = strdup("Failed");
								res->resolved = 0;
							}

							get_du_version(du_ref, &package_version);
							res->version = strdup(package_version ? package_version : "");
							res->du_ref = strdup(du_ref ? du_ref : "");
							res->complete_time = strdup(mix_get_time());
							res->fault = error;
							FREE(du_ref);
							break;

						case DU_UNINSTALL:
							if (p->uuid == NULL || *(p->uuid) == '\0' || !environment_exists(p->executionenvref)) {
								res->fault = FAULT_CPE_UNKNOWN_DEPLOYMENT_UNIT;
								break;
							}

							get_deployment_unit_name_version(p->uuid, &package_name, &package_version, &package_env);
							if (!package_name || *package_name == '\0' || !package_version || *package_version == '\0' || !package_env || *package_env == '\0') {
								res->fault = FAULT_CPE_UNKNOWN_DEPLOYMENT_UNIT;
								break;
							}
							du_ref = (package_name && package_env) ? get_deployment_unit_reference(package_name, package_env) : NULL;
							get_du_version(du_ref, &package_version);
							error = cwmp_launch_du_uninstall(package_name, get_exec_env_name(package_env), get_exec_env_id(package_env), &res);
							if (error == FAULT_CPE_NO_FAULT) {
								res->current_state = strdup("Uninstalled");
								res->resolved = 1;
							} else {
								res->current_state = strdup("Installed");
								res->resolved = 0;
							}

							res->du_ref = strdup(du_ref ? du_ref : "");
							res->uuid = strdup(p->uuid);
							res->version = strdup(package_version);
							res->complete_time = strdup(mix_get_time());
							res->fault = error;
							FREE(du_ref);
							FREE(package_name);
							FREE(package_version);
							FREE(package_env);
							break;
						}
					}

					bkp_session_delete_change_du_state(pchange_du_state);
					bkp_session_save();
					bkp_session_insert_du_state_change_complete(pdu_state_change_complete);
					bkp_session_save();
					cwmp_root_cause_changedustate_complete(cwmp, pdu_state_change_complete);
				}
			}

			pthread_mutex_lock(&mutex_change_du_state);
			pthread_cond_timedwait(&threshold_change_du_state, &mutex_change_du_state, &change_du_state_timeout);
			pthread_mutex_unlock(&mutex_change_du_state);

			pthread_mutex_unlock(&(cwmp->mutex_session_send));
			pthread_cond_signal(&(cwmp->threshold_session_send));

			pthread_mutex_lock(&mutex_change_du_state);
			list_del(&(pchange_du_state->list));
			cwmp_free_change_du_state_request(pchange_du_state);
			pthread_mutex_unlock(&mutex_change_du_state);
			continue;
		} else {
			pthread_mutex_lock(&mutex_change_du_state);
			pthread_cond_wait(&threshold_change_du_state, &mutex_change_du_state);
			pthread_mutex_unlock(&mutex_change_du_state);
		}
	}
	return NULL;
}

int cwmp_rpc_acs_destroy_data_du_state_change_complete(struct session *session __attribute__((unused)), struct rpc *rpc)
{
	if (rpc->extra_data != NULL) {
		struct du_state_change_complete *p;
		p = (struct du_state_change_complete *)rpc->extra_data;
		bkp_session_delete_du_state_change_complete(p);
		bkp_session_save();
		FREE(p->command_key);
	}
	return 0;
}

int cwmp_free_change_du_state_request(struct change_du_state *change_du_state)
{
	if (change_du_state != NULL) {
		struct list_head *ilist, *q;

		list_for_each_safe (ilist, q, &(change_du_state->list_operation)) {
			struct operations *operation = list_entry(ilist, struct operations, list);
			FREE(operation->url);
			FREE(operation->uuid);
			FREE(operation->username);
			FREE(operation->password);
			FREE(operation->version);
			FREE(operation->executionenvref);
			list_del(&(operation->list));
			FREE(operation);
		}
		FREE(change_du_state->command_key);
		FREE(change_du_state);
	}
	return CWMP_OK;
}
