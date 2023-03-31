/*
 * datamodel_interface.h - API to call BBF datamodel functions (set, get, add, delete, setattributes, getattributes, getnames, ...)
 *
 * Copyright (C) 2021-2022, IOPSYS Software Solutions AB.
 *
 *	  Author Omar Kallel <omar.kallel@pivasoftware.com>
 *
 * See LICENSE file for license related information.
 *
 */

#ifndef SRC_DATAMODELIFACE_H_
#define SRC_DATAMODELIFACE_H_

#include "common.h"

#define DM_ROOT_OBJ "Device."
extern unsigned int transaction_id;

bool cwmp_transaction(const char *cmd, bool restart_services);

bool cwmp_get_parameter_value(char *parameter_name, struct cwmp_dm_parameter *dm_parameter);

char *cwmp_get_parameter_values(char *parameter_name, struct list_head *parameters_list);
char *cwmp_get_parameter_names(char *parameter_name, bool next_level, struct list_head *parameters_list);

int cwmp_set_multiple_parameters_values(struct list_head *parameters_values_list, struct list_head *faults_list);

char *cwmp_add_object(char *object_name, char **instance);
char *cwmp_delete_object(char *object_name);



#endif /* SRC_DATAMODELIFACE_H_ */
