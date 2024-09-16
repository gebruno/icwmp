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

#define CDU_TIMEOUT 86400 //24 hours
extern struct list_head list_change_du_state;
extern struct list_head du_uuid_list;

int cwmp_rpc_acs_destroy_data_du_state_change_complete(struct rpc *rpc);
int cwmp_free_change_du_state_request(struct change_du_state *change_du_state);
void change_du_state_execute(struct uloop_timeout *utimeout);
void apply_change_du_state();
void remove_node_from_uuid_list(char *uuid, char *operation);
bool exists_in_uuid_list(char *uuid, char *operation);
void clean_du_uuid_list(void);
#endif
