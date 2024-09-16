/*
 * rpc.h - CWMP RPC methods
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

#ifndef __RPC__SOAP__H_
#define __RPC__SOAP__H_

#include "common.h"
#include "session.h"

extern const struct rpc_cpe_method rpc_cpe_methods[__RPC_CPE_MAX];
extern struct rpc_acs_method rpc_acs_methods[__RPC_ACS_MAX];

int cwmp_handle_rpc_cpe_get_parameter_values(struct rpc *rpc);
int cwmp_handle_rpc_cpe_set_parameter_attributes(struct rpc *rpc);
int cwmp_handle_rpc_cpe_get_parameter_attributes(struct rpc *rpc);
int cwmp_handle_rpc_cpe_add_object(struct rpc *rpc);
int cwmp_handle_rpc_cpe_delete_object(struct rpc *rpc);
int cwmp_rpc_acs_prepare_message_inform(struct rpc *rpc);
int xml_handle_message();
void load_default_forced_inform(void);
void clean_force_inform_list(void);
void load_forced_inform_json(void);

#endif
