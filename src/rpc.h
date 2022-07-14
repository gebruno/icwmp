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

#define MAX_NBRE_CUSTOM_INFORM 256
extern char *custom_forced_inform_parameters[MAX_NBRE_CUSTOM_INFORM];
extern char *boot_inform_parameters[MAX_NBRE_CUSTOM_INFORM];
extern int nbre_custom_inform;
extern int nbre_boot_inform;
extern const struct rpc_cpe_method rpc_cpe_methods[__RPC_CPE_MAX];
extern struct rpc_acs_method rpc_acs_methods[__RPC_ACS_MAX];

int cwmp_handle_rpc_cpe_get_rpc_methods(struct session *session, struct rpc *rpc);
int cwmp_handle_rpc_cpe_set_parameter_values(struct session *session, struct rpc *rpc);
int cwmp_handle_rpc_cpe_get_parameter_values(struct session *session, struct rpc *rpc);
int cwmp_handle_rpc_cpe_get_parameter_names(struct session *session, struct rpc *rpc);
int cwmp_handle_rpc_cpe_set_parameter_attributes(struct session *session, struct rpc *rpc);
int cwmp_handle_rpc_cpe_get_parameter_attributes(struct session *session, struct rpc *rpc);
int cwmp_handle_rpc_cpe_add_object(struct session *session, struct rpc *rpc);
int cwmp_handle_rpc_cpe_delete_object(struct session *session, struct rpc *rpc);
int cwmp_handle_rpc_cpe_reboot(struct session *session, struct rpc *rpc);
int cwmp_handle_rpc_cpe_download(struct session *session, struct rpc *rpc);
int cwmp_handle_rpc_cpe_upload(struct session *session, struct rpc *rpc);
int cwmp_handle_rpc_cpe_factory_reset(struct session *session, struct rpc *rpc);
int cwmp_handle_rpc_cpe_x_factory_reset_soft(struct session *session, struct rpc *rpc);
int cancel_transfer(char *key);
int cwmp_handle_rpc_cpe_cancel_transfer(struct session *session, struct rpc *rpc);
int cwmp_handle_rpc_cpe_schedule_inform(struct session *session, struct rpc *rpc);
int cwmp_handle_rpc_cpe_schedule_download(struct session *session, struct rpc *rpc);
int cwmp_handle_rpc_cpe_change_du_state(struct session *session, struct rpc *rpc);
int cwmp_handle_rpc_cpe_fault(struct session *session, struct rpc *rpc);

int cwmp_rpc_acs_prepare_message_inform(struct cwmp *cwmp, struct session *session, struct rpc *rpc);
int cwmp_rpc_acs_parse_response_inform(struct cwmp *cwmp, struct session *session, struct rpc *rpc);
int cwmp_rpc_acs_parse_response_get_rpc_methods(struct cwmp *cwmp, struct session *session, struct rpc *this);
int cwmp_rpc_acs_prepare_get_rpc_methods(struct cwmp *cwmp, struct session *session, struct rpc *rpc);
int cwmp_rpc_acs_prepare_transfer_complete(struct cwmp *cwmp, struct session *session, struct rpc *rpc);
int cwmp_rpc_acs_prepare_du_state_change_complete(struct cwmp *cwmp, struct session *session, struct rpc *rpc);

int xml_handle_message(struct session *session);
int cwmp_create_fault_message(struct session *session, struct rpc *rpc_cpe, int fault_code);

#endif
