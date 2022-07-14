/*
 * cwmp-du_state.h - ChangeDUState method corresponding functions
 *
 * Copyright (C) 2021-2022, IOPSYS Software Solutions AB.
 *
 *	  Author Omar Kallel <omar.kallel@pivasoftware.com>
 *
 * See LICENSE file for license related information.
 *
 */

#ifndef CWMP_DU_STATE_H
#define CWMP_DU_STATE_H

#include "common.h"

extern struct list_head list_change_du_state;
extern pthread_mutex_t mutex_change_du_state;
extern pthread_cond_t threshold_change_du_state;

int cwmp_du_install(char *url, char *uuid, char *user, char *pass, char *env_name, int env_id, char **fault_code);
int cwmp_du_update(char *url, char *uuid, char *user, char *pass, char *env_name, int env_id, char **fault_code);
int cwmp_du_uninstall(char *package_name, char *env_name, int env_id, char **fault_code);
int cwmp_rpc_acs_destroy_data_du_state_change_complete(struct session *session, struct rpc *rpc);
void *thread_cwmp_rpc_cpe_change_du_state(void *v);
int cwmp_free_change_du_state_request(struct change_du_state *change_du_state);
#endif
