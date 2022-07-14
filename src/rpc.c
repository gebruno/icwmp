/*
 * rpc.c - CWMP RPC methods
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

#include "rpc.h"

#include "download.h"
#include "cwmp_du_state.h"
#include "log.h"
#include "event.h"
#include "datamodel_interface.h"
#include "event.h"
#include "xml.h"
#include "backupSession.h"
#include "notifications.h"
#include "upload.h"
#include "sched_inform.h"
#include "diagnostic.h"

#define PROCESSING_DELAY (1) // In download/upload the message enqueued before sending the response, which cause the download/upload
			     // to start just before the time. This delay is to compensate the time lapsed during the message enqueue and response
#define DM_CONN_REQ_URL "Device.ManagementServer.ConnectionRequestURL"

struct cwmp_namespaces ns;
const struct rpc_cpe_method rpc_cpe_methods[] = { [RPC_CPE_GET_RPC_METHODS] = { "GetRPCMethods", cwmp_handle_rpc_cpe_get_rpc_methods, AMD_1 },
						  [RPC_CPE_SET_PARAMETER_VALUES] = { "SetParameterValues", cwmp_handle_rpc_cpe_set_parameter_values, AMD_1 },
						  [RPC_CPE_GET_PARAMETER_VALUES] = { "GetParameterValues", cwmp_handle_rpc_cpe_get_parameter_values, AMD_1 },
						  [RPC_CPE_GET_PARAMETER_NAMES] = { "GetParameterNames", cwmp_handle_rpc_cpe_get_parameter_names, AMD_1 },
						  [RPC_CPE_SET_PARAMETER_ATTRIBUTES] = { "SetParameterAttributes", cwmp_handle_rpc_cpe_set_parameter_attributes, AMD_1 },
						  [RPC_CPE_GET_PARAMETER_ATTRIBUTES] = { "GetParameterAttributes", cwmp_handle_rpc_cpe_get_parameter_attributes, AMD_1 },
						  [RPC_CPE_ADD_OBJECT] = { "AddObject", cwmp_handle_rpc_cpe_add_object, AMD_1 },
						  [RPC_CPE_DELETE_OBJECT] = { "DeleteObject", cwmp_handle_rpc_cpe_delete_object, AMD_1 },
						  [RPC_CPE_REBOOT] = { "Reboot", cwmp_handle_rpc_cpe_reboot, AMD_1 },
						  [RPC_CPE_DOWNLOAD] = { "Download", cwmp_handle_rpc_cpe_download, AMD_1 },
						  [RPC_CPE_UPLOAD] = { "Upload", cwmp_handle_rpc_cpe_upload, AMD_1 },
						  [RPC_CPE_FACTORY_RESET] = { "FactoryReset", cwmp_handle_rpc_cpe_factory_reset, AMD_1 },
						  [RPC_CPE_CANCEL_TRANSFER] = { "CancelTransfer", cwmp_handle_rpc_cpe_cancel_transfer, AMD_3 },
						  [RPC_CPE_SCHEDULE_INFORM] = { "ScheduleInform", cwmp_handle_rpc_cpe_schedule_inform, AMD_1 },
						  [RPC_CPE_SCHEDULE_DOWNLOAD] = { "ScheduleDownload", cwmp_handle_rpc_cpe_schedule_download, AMD_3 },
						  [RPC_CPE_CHANGE_DU_STATE] = { "ChangeDUState", cwmp_handle_rpc_cpe_change_du_state, AMD_3 },
						  [RPC_CPE_X_FACTORY_RESET_SOFT] = { "X_FactoryResetSoft", cwmp_handle_rpc_cpe_x_factory_reset_soft, AMD_1 },
						  [RPC_CPE_FAULT] = { "Fault", cwmp_handle_rpc_cpe_fault, AMD_1 } };

struct rpc_acs_method rpc_acs_methods[] = { [RPC_ACS_INFORM] = { "Inform", cwmp_rpc_acs_prepare_message_inform, cwmp_rpc_acs_parse_response_inform, NULL, NOT_KNOWN },
						  [RPC_ACS_GET_RPC_METHODS] = { "GetRPCMethods", cwmp_rpc_acs_prepare_get_rpc_methods, cwmp_rpc_acs_parse_response_get_rpc_methods, NULL, NOT_KNOWN },
						  [RPC_ACS_TRANSFER_COMPLETE] = { "TransferComplete", cwmp_rpc_acs_prepare_transfer_complete, NULL, cwmp_rpc_acs_destroy_data_transfer_complete, NOT_KNOWN },
						  [RPC_ACS_DU_STATE_CHANGE_COMPLETE] = { "DUStateChangeComplete", cwmp_rpc_acs_prepare_du_state_change_complete, NULL, cwmp_rpc_acs_destroy_data_du_state_change_complete, NOT_KNOWN }
};

char *custom_forced_inform_parameters[MAX_NBRE_CUSTOM_INFORM] = { 0 };
char *boot_inform_parameters[MAX_NBRE_CUSTOM_INFORM] = { 0 };
int nbre_custom_inform = 0;
int nbre_boot_inform = 0;
char *forced_inform_parameters[] = {
	"Device.RootDataModelVersion",
	"Device.DeviceInfo.HardwareVersion",
	"Device.DeviceInfo.SoftwareVersion",
	"Device.DeviceInfo.ProvisioningCode",
	"Device.ManagementServer.ParameterKey",
	DM_CONN_REQ_URL,
	"Device.ManagementServer.AliasBasedAddressing"
};

int xml_handle_message(struct session *session)
{
	struct rpc *rpc_cpe;
	char *c;
	int i;
	mxml_node_t *b;
	struct cwmp *cwmp = &cwmp_main;
	struct config *conf;
	conf = &(cwmp->conf);

	/* get method */
	if (icwmp_asprintf(&c, "%s:%s", ns.soap_env, "Body") == -1) {
		CWMP_LOG(INFO, "Internal error");
		session->fault_code = FAULT_CPE_INTERNAL_ERROR;
		goto fault;
	}
	b = mxmlFindElement(session->tree_in, session->tree_in, c, NULL, NULL, MXML_DESCEND);

	if (!b) {
		CWMP_LOG(INFO, "Invalid received message");
		session->fault_code = FAULT_CPE_REQUEST_DENIED;
		goto fault;
	}
	session->body_in = b;

	while (1) {
		b = mxmlWalkNext(b, session->body_in, MXML_DESCEND_FIRST);
		if (!b)
			goto error;
		if (mxmlGetType(b) == MXML_ELEMENT)
			break;
	}

	c = (char *)mxmlGetElement(b);
	/* convert QName to localPart, check that ns is the expected one */
	if (strchr(c, ':')) {
		char *tmp = strchr(c, ':');
		size_t ns_len = tmp - c;

		if (strlen(ns.cwmp) != ns_len) {
			CWMP_LOG(INFO, "Invalid received message");
			session->fault_code = FAULT_CPE_REQUEST_DENIED;
			goto fault;
		}

		if (strncmp(ns.cwmp, c, ns_len)) {
			CWMP_LOG(INFO, "Invalid received message");
			session->fault_code = FAULT_CPE_REQUEST_DENIED;
			goto fault;
		}

		c = tmp + 1;
	} else {
		CWMP_LOG(INFO, "Invalid received message");
		session->fault_code = FAULT_CPE_REQUEST_DENIED;
		goto fault;
	}
	CWMP_LOG(INFO, "SOAP RPC message: %s", c);
	rpc_cpe = NULL;
	for (i = 1; i < __RPC_CPE_MAX; i++) {
		if (i != RPC_CPE_FAULT && strcmp(c, rpc_cpe_methods[i].name) == 0 && rpc_cpe_methods[i].amd <= conf->supported_amd_version) {
			CWMP_LOG(INFO, "%s RPC is supported", c);
			rpc_cpe = cwmp_add_session_rpc_cpe(session, i);
			if (rpc_cpe == NULL)
				goto error;
			break;
		}
	}
	if (!rpc_cpe) {
		CWMP_LOG(INFO, "%s RPC is not supported", c);
		session->fault_code = FAULT_CPE_METHOD_NOT_SUPPORTED;
		goto fault;
	}
	return 0;
fault:
	rpc_cpe = cwmp_add_session_rpc_cpe(session, RPC_CPE_FAULT);
	if (rpc_cpe == NULL)
		goto error;
	return 0;
error:
	return -1;
}

/*
 * [RPC ACS]: Inform
 */
static int xml_prepare_parameters_inform(struct cwmp_dm_parameter *dm_parameter, mxml_node_t *parameter_list, int *size)
{
	mxml_node_t *node = NULL, *b;
	b = mxmlFindElementOpaque(parameter_list, parameter_list, dm_parameter->name, MXML_DESCEND);
	if (b && dm_parameter->value != NULL) {
		node = mxmlGetParent(b);
		b = mxmlFindElement(node, node, "Value", NULL, NULL, MXML_DESCEND_FIRST);
		if (!b)
			return 0;
		mxml_node_t *c = mxmlGetFirstChild(b);
		if (c && strcmp(dm_parameter->value, mxmlGetOpaque(c)) == 0)
			return 0;
		mxmlDelete(b);
		(*size)--;
	} else if (dm_parameter->value == NULL)
		return 0;

	char *type = (dm_parameter->type && dm_parameter->type[0] != '\0') ? dm_parameter->type : "xsd:string";
	if (node == NULL) {
		struct xml_data_struct inform_params_xml_attrs = {0};
		struct xml_list_data *xml_data = calloc(1, sizeof(struct xml_list_data));
		xml_data->param_name = strdup(dm_parameter->name);
		xml_data->param_value = strdup(dm_parameter->value);
		xml_data->param_type = strdup(type);
		LIST_HEAD(prameters_xml_list);
		list_add_tail(&xml_data->list, &prameters_xml_list);
		inform_params_xml_attrs.data_list = &prameters_xml_list;
		int fault = build_xml_node_data(SOAP_PARAM_STRUCT, parameter_list, &inform_params_xml_attrs);
		if (fault != CWMP_OK)
			return -1;

		cwmp_free_all_xml_data_list(&prameters_xml_list);
	} else {
		struct xml_data_struct inform_param_value_xml_attrs = {0};
		inform_param_value_xml_attrs.value = &dm_parameter->value;
		inform_param_value_xml_attrs.xsi_type = &type;

		int fault = build_xml_node_data(SOAP_VALUE_STRUCT, node, &inform_param_value_xml_attrs);
		if (fault != CWMP_OK)
			return -1;
	}

	(*size)++;
	return 0;
}

static void load_inform_xml_schema(mxml_node_t **tree, struct cwmp *cwmp, struct session *session)
{
	char declaration[1024] = {0};
	mxml_node_t *xml = NULL, *envelope = NULL;
	if (tree == NULL)
		return;

	*tree = NULL;

	snprintf(declaration, sizeof(declaration), "?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?");

	xml= mxmlNewElement(NULL, declaration);
	if (xml == NULL)
		return;

	struct xml_data_struct env_xml_attrs = {0};

	env_xml_attrs.xml_env = &envelope;
	env_xml_attrs.amd_version = &cwmp->conf.supported_amd_version;
	env_xml_attrs.session_timeout = &cwmp->conf.session_timeout;

	int fault = build_xml_node_data(SOAP_ENV, xml, &env_xml_attrs);

	if (envelope == NULL || fault != CWMP_OK) {
		MXML_DELETE(xml);
		return;
	}

	mxml_node_t *inform = build_top_body_soap_request(envelope, "Inform");
	if (inform == NULL) {
		MXML_DELETE(xml);
		return;
	}

	struct xml_data_struct inform_xml_attrs = {0};

	char *manufacturer = cwmp->deviceid.manufacturer ? cwmp->deviceid.manufacturer : "";
	char *oui = cwmp->deviceid.oui ? cwmp->deviceid.oui : "";
	char *product_class = cwmp->deviceid.productclass ? cwmp->deviceid.productclass : "";
	char *serial_number = cwmp->deviceid.serialnumber ? cwmp->deviceid.serialnumber : "";
	int max_env = 1;
	char *current_time = get_time(time(NULL));

	inform_xml_attrs.manufacturer = &manufacturer;
	inform_xml_attrs.oui = &oui;
	inform_xml_attrs.product_class = &product_class;
	inform_xml_attrs.serial_number = &serial_number;
	inform_xml_attrs.max_envelopes = &max_env;
	inform_xml_attrs.current_time = &current_time;
	inform_xml_attrs.retry_count = &cwmp->retry_count_session;

	LIST_HEAD(xml_events_list);
	event_container_list_to_xml_data_list(&(session->head_event_container), &xml_events_list);
	inform_xml_attrs.data_list = &xml_events_list;

	fault = build_xml_node_data(SOAP_INFORM_CWMP, inform, &inform_xml_attrs);
	if (fault != CWMP_OK) {
		MXML_DELETE(xml);
		return;
	}

	cwmp_free_all_xml_data_list(&xml_events_list);
	mxml_node_t *param_list = mxmlNewElement(inform, "ParameterList");
	if (param_list == NULL) {
		MXML_DELETE(xml);
		return;
	}

	mxmlElementSetAttr(param_list, "soap_enc:arrayType", "cwmp:ParameterValueStruct[0]");

	struct list_head *ilist, *jlist;
	struct cwmp_dm_parameter *dm_parameter;
	int size = 0;

	list_for_each (ilist, &(session->head_event_container)) {
		struct event_container *event_container = list_entry(ilist, struct event_container, list);
		list_for_each (jlist, &(event_container->head_dm_parameter)) {
			dm_parameter = list_entry(jlist, struct cwmp_dm_parameter, list);
			if (xml_prepare_parameters_inform(dm_parameter, param_list, &size)) {
				MXML_DELETE(xml);
				return;
			}
		}
	}

	size_t inform_parameters_nbre = sizeof(forced_inform_parameters) / sizeof(forced_inform_parameters[0]);
	size_t i;
	int j;
	struct cwmp_dm_parameter cwmp_dm_param = { 0 };
	LIST_HEAD(list_inform);
	for (i = 0; i < inform_parameters_nbre; i++) {
		char *fault = cwmp_get_single_parameter_value(forced_inform_parameters[i], &cwmp_dm_param);
		if (fault != NULL)
			continue;

		// An empty connection url cause CDR test to break
		if (strcmp(forced_inform_parameters[i], DM_CONN_REQ_URL) == 0 && cwmp_dm_param.value != NULL && strlen(cwmp_dm_param.value) == 0) {
			CWMP_LOG(ERROR, "# Empty CR URL[%s] value", forced_inform_parameters[i]);
			MXML_DELETE(xml);
			return;
		}

		if (xml_prepare_parameters_inform(&cwmp_dm_param, param_list, &size)) {
			MXML_DELETE(xml);
			return;
		}
	}

	for (j = 0; j < nbre_custom_inform; j++) {
		char *fault = cwmp_get_single_parameter_value(custom_forced_inform_parameters[j], &cwmp_dm_param);
		if (fault != NULL)
			continue;
		if (xml_prepare_parameters_inform(&cwmp_dm_param, param_list, &size)) {
			MXML_DELETE(xml);
			return;
		}
	}

	if (cwmp->is_boot == true) {
		for (j = 0; j < nbre_boot_inform; j++) {
			char *fault = cwmp_get_single_parameter_value(boot_inform_parameters[j], &cwmp_dm_param);
			if (fault != NULL)
				continue;
			if (xml_prepare_parameters_inform(&cwmp_dm_param, param_list, &size)) {
				MXML_DELETE(xml);
				return;
			}
		}
	}

	char c[256] = {0};
	if (snprintf(c, sizeof(c), "cwmp:ParameterValueStruct[%d]", size) == -1) {
		MXML_DELETE(xml);
		return;
	}

	mxmlElementSetAttr(param_list, "xsi:type", "soap_enc:Array");
	mxmlElementSetAttr(param_list, "soap_enc:arrayType", c);

	*tree = xml;
}

int cwmp_rpc_acs_prepare_message_inform(struct cwmp *cwmp, struct session *session, struct rpc *this)
{
	mxml_node_t *tree;

	if (session == NULL || this == NULL)
		return -1;

	load_inform_xml_schema(&tree, cwmp, session);

	if (!tree)
		goto error;

	session->tree_out = tree;

	return 0;

error:
	CWMP_LOG(ERROR, "Unable Prepare Message Inform", CWMP_BKP_FILE);
	return -1;
}

int cwmp_rpc_acs_parse_response_inform(struct cwmp *cwmp, struct session *session, struct rpc *this __attribute__((unused)))
{
	mxml_node_t *tree, *b;
	int i = -1;
	char *c;
	const char *cwmp_urn;

	tree = session->tree_in;
	if (!tree)
		goto error;
	b = mxmlFindElement(tree, tree, "MaxEnvelopes", NULL, NULL, MXML_DESCEND);
	if (!b)
		goto error;
	b = mxmlWalkNext(b, tree, MXML_DESCEND_FIRST);
	if (!b || mxmlGetType(b) != MXML_OPAQUE || !mxmlGetOpaque(b))
		goto error;
	if (cwmp->conf.supported_amd_version == 1) {
		cwmp->conf.amd_version = 1;
		return 0;
	}
	b = mxmlFindElement(tree, tree, "UseCWMPVersion", NULL, NULL, MXML_DESCEND);
	if (b && cwmp->conf.supported_amd_version >= 5) { //IF supported version !=5 acs response dosen't contain UseCWMPVersion
		b = mxmlWalkNext(b, tree, MXML_DESCEND_FIRST);
		if (!b || mxmlGetType(b) != MXML_OPAQUE || !mxmlGetOpaque(b))
			goto error;
		c = (char *) mxmlGetOpaque(b);
		if (c && *(c + 1) == '.') {
			c += 2;
			cwmp->conf.amd_version = atoi(c) + 1;
			return 0;
		}
		goto error;
	}
	for (i = 0; cwmp_urls[i] != NULL; i++) {
		cwmp_urn = cwmp_urls[i];
		c = (char *)xml__get_attribute_name_by_value(tree, cwmp_urn);
		if (c && *(c + 5) == ':') {
			break;
		}
	}
	if (i == 0) {
		cwmp->conf.amd_version = i + 1;
	} else if (i >= 1 && i <= 3) {
		switch (cwmp->conf.supported_amd_version) {
		case 1:
			cwmp->conf.amd_version = 1; //Already done
			break;
		case 2:
		case 3:
		case 4:
			//MIN ACS CPE
			if (cwmp->conf.supported_amd_version <= i + 1)
				cwmp->conf.amd_version = cwmp->conf.supported_amd_version;
			else
				cwmp->conf.amd_version = i + 1;
			break;
		//(cwmp->supported_conf.amd_version < i+1) ?"cwmp->conf.amd_version":"i+1";
		case 5:
			cwmp->conf.amd_version = i + 1;
			break;
		}
	} else if (i >= 4) {
		cwmp->conf.amd_version = cwmp->conf.supported_amd_version;
	}
	return 0;

error:
	return -1;
}

int set_rpc_acs_to_supported(char *rpc_name)
{
	int i;

	for (i=1; i < __RPC_ACS_MAX; i++) {
		if (strcmp(rpc_acs_methods[i].name, rpc_name) == 0) {
			rpc_acs_methods[i].acs_support = RPC_ACS_SUPPORT;
			return i;
		}
	}
	return -1;
}

void set_not_known_acs_support()
{
	int i;
	for (i=1; i < __RPC_ACS_MAX; i++) {
		if ((i != RPC_ACS_INFORM) && (rpc_acs_methods[i].acs_support == NOT_KNOWN))
			rpc_acs_methods[i].acs_support = RPC_ACS_NOT_SUPPORT;
	}
}

int cwmp_rpc_acs_parse_response_get_rpc_methods(struct cwmp *cwmp __attribute__((unused)), struct session *session, struct rpc *this __attribute__((unused)))
{
	mxml_node_t *tree, *b;
	tree = session->tree_in;
	b = mxmlFindElement(tree, tree, "cwmp:GetRPCMethodsResponse", NULL, NULL, MXML_DESCEND);
	if (!b)
		goto error;

	while (b) {
			const char *node_opaque = mxmlGetOpaque(b);
			mxml_node_t *parent_node = mxmlGetParent(b);
			mxml_type_t node_type = mxmlGetType(b);

			if (node_type == MXML_OPAQUE && mxmlGetType(parent_node) == MXML_ELEMENT && node_opaque && strcmp((char *) mxmlGetElement(parent_node), "string") == 0)
				set_rpc_acs_to_supported((char*)node_opaque);

			b = mxmlWalkNext(b, session->body_in, MXML_DESCEND);
	}
	set_not_known_acs_support();
	return 0;
error:
	return -1;
}

/*
 * [RPC ACS]: GetRPCMethods
 */
int cwmp_rpc_acs_prepare_get_rpc_methods(struct cwmp *cwmp, struct session *session, struct rpc *rpc __attribute__((unused)))
{
	mxml_node_t *tree, *n;

	load_response_xml_schema(&tree);
	if (!tree)
		return -1;

	n = mxmlFindElement(tree, tree, "soap_env:Envelope", NULL, NULL, MXML_DESCEND);
	if (!n)
		return -1;
	mxmlElementSetAttr(n, "xmlns:cwmp", cwmp_urls[(cwmp->conf.amd_version) - 1]);

	n = build_top_body_soap_request(tree, "GetRPCMethods");
	if (!n)
		return -1;

	session->tree_out = tree;

	return 0;
}

/*
 * [RPC ACS]: TransferComplete
 */
int cwmp_rpc_acs_prepare_transfer_complete(struct cwmp *cwmp, struct session *session, struct rpc *rpc)
{
	mxml_node_t *tree, *n;
	struct transfer_complete *p;

	p = (struct transfer_complete *)rpc->extra_data;
	load_response_xml_schema(&tree);
	if (!tree)
		goto error;

	n = mxmlFindElement(tree, tree, "soap_env:Envelope", NULL, NULL, MXML_DESCEND);
	if (!n)
		goto error;
	mxmlElementSetAttr(n, "xmlns:cwmp", cwmp_urls[(cwmp->conf.amd_version) - 1]);

	n = build_top_body_soap_request(tree, "TransferComplete");
	if (!n)
		goto error;

	struct xml_data_struct transfer_complete_xml_attrs = {0};

	transfer_complete_xml_attrs.command_key = &p->command_key;
	transfer_complete_xml_attrs.start_time = &p->start_time;
	transfer_complete_xml_attrs.complete_time = &p->complete_time;
	int faultcode = p->fault_code ? atoi(FAULT_CPE_ARRAY[p->fault_code].CODE) : 0;
	transfer_complete_xml_attrs.fault_code = &faultcode;
	char *faultstring = strdup(p->fault_code ? FAULT_CPE_ARRAY[p->fault_code].DESCRIPTION : "");
	transfer_complete_xml_attrs.fault_string = &faultstring;

	int fault = build_xml_node_data(SOAP_ACS_TRANSCOMPLETE, n, &transfer_complete_xml_attrs);
	if (fault != CWMP_OK)
		goto error;

	FREE(faultstring);
	session->tree_out = tree;

	return 0;

error:
	return -1;
}

/*
 * [RPC ACS]: DUStateChangeComplete
 */
int cwmp_rpc_acs_prepare_du_state_change_complete(struct cwmp *cwmp, struct session *session, struct rpc *rpc)
{
	mxml_node_t *tree, *n;
	struct du_state_change_complete *p;

	p = (struct du_state_change_complete *)rpc->extra_data;
	load_response_xml_schema(&tree);
	if (!tree)
		goto error;

	n = mxmlFindElement(tree, tree, "soap_env:Envelope", NULL, NULL, MXML_DESCEND);
	if (!n)
		goto error;

	mxmlElementSetAttr(n, "xmlns:cwmp", cwmp_urls[(cwmp->conf.amd_version) - 1]);

	n = build_top_body_soap_request(tree, "DUStateChangeComplete");
	if (!n)
		goto error;

	LIST_HEAD(opt_result_list);
	cdu_operations_list_to_xml_data_list(&p->list_opresult, &opt_result_list);

	struct xml_data_struct cdu_complete_xml_attrs = {0};

	cdu_complete_xml_attrs.command_key = &p->command_key;
	cdu_complete_xml_attrs.data_list = &opt_result_list;

	int fault = build_xml_node_data(SOAP_DU_CHANGE_COMPLETE, n, &cdu_complete_xml_attrs);
	if (fault != CWMP_OK) {
		cwmp_free_all_xml_data_list(&opt_result_list);
		goto error;
	}

	cwmp_free_all_xml_data_list(&opt_result_list);
	session->tree_out = tree;
	return 0;

error:
	return -1;
}

/*
 * [RPC CPE]: GetParameterValues
 */
int cwmp_handle_rpc_cpe_get_parameter_values(struct session *session, struct rpc *rpc)
{
	mxml_node_t *b, *parameter_list = NULL;
	int fault_code = FAULT_CPE_INTERNAL_ERROR;
	int counter = 0;
	char c[256];

	if (session->tree_out == NULL)
		goto fault;

	b = build_top_body_soap_response(session->tree_out, "GetParameterValues");

	struct xml_data_struct gpv_resp_xml_attrs = {0};

	char *xsi_type = "soap_enc:Array";
	gpv_resp_xml_attrs.xsi_type = &xsi_type;
	gpv_resp_xml_attrs.parameter_list = &parameter_list;

	int fault = build_xml_node_data(SOAP_RESP_GPV, b, &gpv_resp_xml_attrs);
	if (fault != CWMP_OK)
		goto fault;

	LIST_HEAD(gpv_xml_data_list);

	struct xml_data_struct gpv_xml_attrs = {0};
	gpv_xml_attrs.data_list = &gpv_xml_data_list;
	struct xml_tag_validation gpv_validation[] = {{"string", VALIDATE_STR_SIZE, 0, 256}};
	gpv_xml_attrs.validations = gpv_validation;
	gpv_xml_attrs.nbre_validations = 1;

	fault = load_xml_node_data(SOAP_REQ_GPV, session->body_in, &gpv_xml_attrs);
	if (fault) {
		fault_code = fault;
		goto fault;
	}

	struct xml_list_data *p = NULL;
	struct list_head *l = gpv_xml_data_list.next;
	while (l != &gpv_xml_data_list) {
		p = list_entry(l, struct xml_list_data, list);
		LIST_HEAD(parameters_list);
		char *err = cwmp_get_parameter_values(p->param_name, &parameters_list);
		if (err && !is_obj_excluded(p->param_name)) {
			fault_code = cwmp_get_fault_code_by_string(err);
			goto fault;
		}
		LIST_HEAD(prameters_xml_list);
		dm_parameter_list_to_xml_data_list(&parameters_list, &prameters_xml_list);

		struct xml_data_struct prmvalstrct_resp_xml_attrs = {0};
		prmvalstrct_resp_xml_attrs.counter = &counter;
		prmvalstrct_resp_xml_attrs.data_list = &prameters_xml_list;

		fault = build_xml_node_data(SOAP_PARAM_STRUCT, parameter_list, &prmvalstrct_resp_xml_attrs);
		if (fault != CWMP_OK)
			goto fault;

		cwmp_free_all_dm_parameter_list(&parameters_list);
		cwmp_free_all_xml_data_list(&prameters_xml_list);
		l = l->next;
	}
	cwmp_free_all_xml_data_list(&gpv_xml_data_list);
	b = mxmlFindElement(session->tree_out, session->tree_out, "ParameterList", NULL, NULL, MXML_DESCEND);
	if (!b)
		goto fault;

	if (snprintf(c, sizeof(c), "cwmp:ParameterValueStruct[%d]", counter) == -1)
		goto fault;

	mxmlElementSetAttr(b, "soap_enc:arrayType", c);

	return 0;

fault:
	if (cwmp_create_fault_message(session, rpc, fault_code))
		return -1;
	return 0;
}

/*
 * [RPC CPE]: GetParameterNames
 */
int cwmp_handle_rpc_cpe_get_parameter_names(struct session *session, struct rpc *rpc)
{
	mxml_node_t *n, *b, *parameter_list;
	char *parameter_name = NULL;
	bool next_level = true;
	int counter = 0, fault_code = FAULT_CPE_INTERNAL_ERROR;
	LIST_HEAD(parameters_list);
	char c[256];

	struct xml_data_struct gpn_xml_attrs = {0};

	gpn_xml_attrs.next_level = &next_level;
	gpn_xml_attrs.parameter_path = &parameter_name;
	struct xml_tag_validation gpn_validation[] = {{"ParameterPath", VALIDATE_STR_SIZE, 0, 256}, {"NextLevel", VALIDATE_BOOLEAN, 0, 0}};
	gpn_xml_attrs.validations = gpn_validation;
	gpn_xml_attrs.nbre_validations = 2;

	int fault = load_xml_node_data(SOAP_REQ_GPN, session->body_in, &gpn_xml_attrs);
	if (fault != CWMP_OK) {
		fault_code = fault;
		goto fault;
	}
	char *err = cwmp_get_parameter_names(parameter_name ? parameter_name : "", next_level, &parameters_list);
	if (err && !is_obj_excluded(parameter_name)) {
		fault_code = cwmp_get_fault_code_by_string(err);
		goto fault;
	}
	FREE(parameter_name);

	if (session->tree_out == NULL)
		goto fault;

	n = build_top_body_soap_response(session->tree_out, "GetParameterNames");

	if (!n){
		fault_code = FAULT_CPE_INTERNAL_ERROR;
		goto fault;
	}

	parameter_list = mxmlNewElement(n, "ParameterList");
	if (!parameter_list){
		fault_code = FAULT_CPE_INTERNAL_ERROR;
		goto fault;
	}

	mxmlElementSetAttr(parameter_list, "xsi:type", "soap_enc:Array");

	LIST_HEAD(prameters_xml_list);
	dm_parameter_list_to_xml_data_list(&parameters_list, &prameters_xml_list);

	struct xml_data_struct gpv_resp_xml_attrs = {0};
	gpv_resp_xml_attrs.data_list = &prameters_xml_list;
	gpv_resp_xml_attrs.counter = &counter;

	fault = build_xml_node_data(SOAP_RESP_GPN, parameter_list, &gpv_resp_xml_attrs);
	if (fault != CWMP_OK)
		goto fault;

	cwmp_free_all_dm_parameter_list(&parameters_list);
	cwmp_free_all_xml_data_list(&prameters_xml_list);

	b = mxmlFindElement(session->tree_out, session->tree_out, "ParameterList", NULL, NULL, MXML_DESCEND);
	if (!b)
		goto fault;

	if (snprintf(c, sizeof(c), "cwmp:ParameterInfoStruct[%d]", counter) == -1)
		goto fault;

	mxmlElementSetAttr(b, "soap_enc:arrayType", c);
	return 0;

fault:
	cwmp_free_all_dm_parameter_list(&parameters_list);
	if (cwmp_create_fault_message(session, rpc, fault_code))
		return -1;
	return 0;
}

/*
 * [RPC CPE]: GetParameterAttributes
 */
int cwmp_handle_rpc_cpe_get_parameter_attributes(struct session *session, struct rpc *rpc)
{
	mxml_node_t *n, *parameter_list, *b;
	int counter = 0, fault_code = FAULT_CPE_INTERNAL_ERROR;
	char c[256];
	b = session->body_in;

	n = build_top_body_soap_response(session->tree_out, "GetParameterAttributes");
	if (!n)
		goto fault;

	parameter_list = mxmlNewElement(n, "ParameterList");
	if (!parameter_list)
		goto fault;

	mxmlElementSetAttr(parameter_list, "xsi:type", "soap_enc:Array");

	LIST_HEAD(gpa_xml_data_list);


	struct xml_data_struct gpa_xml_attrs = {0};
	gpa_xml_attrs.data_list = &gpa_xml_data_list;
	struct xml_tag_validation gpa_validation[] = {{"string", VALIDATE_STR_SIZE, 0, 256}};
	gpa_xml_attrs.validations = gpa_validation;
	gpa_xml_attrs.nbre_validations = 1;

	int fault = load_xml_node_data(SOAP_REQ_GPA, b, &gpa_xml_attrs);
	if (fault) {
		fault_code = fault;
		goto fault;
	}

	struct xml_list_data *p = NULL;
	struct list_head*l= gpa_xml_data_list.next;
	while (l != &gpa_xml_data_list) {
		p = list_entry(l, struct xml_list_data, list);
		LIST_HEAD(parameters_list);
		char *err = cwmp_get_parameter_attributes(p->param_name, &parameters_list);
		if (err && !is_obj_excluded(p->param_name)) {
			fault_code = cwmp_get_fault_code_by_string(err);
			cwmp_free_all_dm_parameter_list(&parameters_list);
			cwmp_free_all_xml_data_list(&gpa_xml_data_list);
			goto fault;
		}
		LIST_HEAD(parameters_xml_list);
		dm_parameter_list_to_xml_data_list(&parameters_list, &parameters_xml_list);

		struct xml_data_struct gpv_resp_xml_attrs = {0};
		gpv_resp_xml_attrs.counter = &counter;
		gpv_resp_xml_attrs.data_list = &parameters_xml_list;
		fault = build_xml_node_data(SOAP_RESP_GPA, parameter_list, &gpv_resp_xml_attrs);
		if (fault != CWMP_OK)
			goto fault;

		cwmp_free_all_xml_data_list(&parameters_xml_list);
		l = l->next;
	}
	cwmp_free_all_xml_data_list(&gpa_xml_data_list);

	b = mxmlFindElement(session->tree_out, session->tree_out, "ParameterList", NULL, NULL, MXML_DESCEND);
	if (!b)
		goto fault;

	if (snprintf(c, sizeof(c), "cwmp:ParameterAttributeStruct[%d]", counter) == -1)
		goto fault;

	mxmlElementSetAttr(b, "soap_enc:arrayType", c);
	return 0;

fault:
	if (cwmp_create_fault_message(session, rpc, fault_code))
		return -1;
	return 0;
}

/*
 * [RPC CPE]: SetParameterValues
 */
int is_duplicated_parameter(mxml_node_t *param_node, struct session *session)
{
	mxml_node_t *b = param_node;
	while ((b = mxmlWalkNext(b, session->body_in, MXML_DESCEND))) {
		const char *node_opaque = mxmlGetOpaque(b);
		mxml_node_t *parent = mxmlGetParent(b);
		mxml_type_t node_type = mxmlGetType(b);

		if (node_type == MXML_OPAQUE && node_opaque && mxmlGetType(parent) == MXML_ELEMENT && !strcmp(mxmlGetElement(parent), "Name")) {
			if (strcmp(node_opaque, mxmlGetOpaque(param_node)) == 0)
				return -1;
		}
	}
	return 0;
}

int cwmp_handle_rpc_cpe_set_parameter_values(struct session *session, struct rpc *rpc)
{
	mxml_node_t *b = NULL;
	char *parameter_key = NULL;
	int fault_code = FAULT_CPE_INTERNAL_ERROR, ret = 0;

	LIST_HEAD(xml_list_set_param_value);
	LIST_HEAD(list_set_param_value);
	LIST_HEAD(list_fault_param);

	rpc->list_set_value_fault = &list_fault_param;
	struct xml_tag_validation spv_validation[] = {{"ParameterKey", VALIDATE_STR_SIZE, 0, 32}, {"Name", VALIDATE_STR_SIZE, 0, 256}};
	struct xml_data_struct spv_xml_attrs = {0};
	spv_xml_attrs.parameter_key = &parameter_key;
	spv_xml_attrs.data_list = &xml_list_set_param_value;
	spv_xml_attrs.validations = spv_validation;
	spv_xml_attrs.nbre_validations = 2;

	int fault = load_xml_node_data(SOAP_REQ_SPV, session->body_in, &spv_xml_attrs);
	if (fault) {
		fault_code = fault;
		goto fault;
	}

	xml_data_list_to_dm_parameter_list(&xml_list_set_param_value, &list_set_param_value);

	int flag = 0;
	if (transaction_id == 0) {
		if (!cwmp_transaction_start("cwmp")) {
			fault_code = FAULT_CPE_INTERNAL_ERROR;
			goto fault;
		}
	}

	fault_code = cwmp_set_multiple_parameters_values(&list_set_param_value, parameter_key ? parameter_key : "", &flag, rpc->list_set_value_fault);
	if (fault_code != FAULT_CPE_NO_FAULT)
		goto fault;

	FREE(parameter_key);
	struct cwmp_dm_parameter *param_value;
	// cppcheck-suppress unknownMacro
	list_for_each_entry (param_value, &list_set_param_value, list)
		set_diagnostic_parameter_structure_value(param_value->name, param_value->value);

	cwmp_free_all_xml_data_list(&xml_list_set_param_value);
	cwmp_free_all_dm_parameter_list(&list_set_param_value);

	b = build_top_body_soap_response(session->tree_out, "SetParameterValues");

	if (!b) {
		fault_code = FAULT_CPE_INTERNAL_ERROR;
		goto fault;
	}

	int status = 1;

	struct xml_data_struct spv_resp_xml_attrs = {.status = &status};
	fault = build_xml_node_data(SOAP_RESP_SPV, b, &spv_resp_xml_attrs);
	if (fault)
		goto fault;

	if (!cwmp_transaction_commit()) {
		fault_code = FAULT_CPE_INTERNAL_ERROR;
		goto fault;
	}

	cwmp_set_end_session(flag | END_SESSION_RESTART_SERVICES | END_SESSION_SET_NOTIFICATION_UPDATE | END_SESSION_RELOAD);
	return 0;

fault:
	cwmp_free_all_dm_parameter_list(&list_set_param_value);
	if (cwmp_create_fault_message(session, rpc, fault_code))
		ret = CWMP_XML_ERR;

	cwmp_free_all_list_param_fault(rpc->list_set_value_fault);
	if (transaction_id) {
		cwmp_transaction_abort();
		transaction_id = 0;
	}
	return ret;
}

/*
 * [RPC CPE]: SetParameterAttributes
 */
int cwmp_handle_rpc_cpe_set_parameter_attributes(struct session *session, struct rpc *rpc)
{
	mxml_node_t *n;
	int fault_code = FAULT_CPE_INTERNAL_ERROR, ret = 0;
	char c[256];

	if (snprintf(c, sizeof(c), "%s:%s", ns.cwmp, "SetParameterAttributes") == -1)
		goto fault;

	n = mxmlFindElement(session->tree_in, session->tree_in, c, NULL, NULL, MXML_DESCEND);

	if (!n)
		goto fault;

	LIST_HEAD(prameters_xml_list);
	struct xml_data_struct spa_xml_attrs = {0};
	spa_xml_attrs.data_list = &prameters_xml_list;
	struct xml_tag_validation spa_validation[] = {{"Name", VALIDATE_STR_SIZE, 0, 256}, {"NotificationChange", VALIDATE_BOOLEAN, 0, 0}, {"Notification", VALIDATE_INT_RANGE, 0, 6}};
	spa_xml_attrs.validations = spa_validation;
	spa_xml_attrs.nbre_validations = 3;

	fault_code = load_xml_node_data(SOAP_REQ_SPA, n, &spa_xml_attrs);
	if (fault_code)
		goto fault;
	struct list_head *l = prameters_xml_list.next;
	struct xml_list_data *p = NULL;
	while (l != &prameters_xml_list) {
		p = list_entry(l, struct xml_list_data, list);
		if (p->param_name && p->notification_change) {
			char *err = cwmp_set_parameter_attributes(p->param_name, p->notification);
			if (err) {
				fault_code = cwmp_get_fault_code_by_string(err);
				goto fault;
			}
		}
		l = l->next;
	}
	cwmp_free_all_xml_data_list(&prameters_xml_list);

	mxml_node_t *resp = build_top_body_soap_response(session->tree_out, "SetParameterAttributes");
	if (!resp)
		goto fault;

	cwmp_set_end_session(END_SESSION_SET_NOTIFICATION_UPDATE | END_SESSION_RESTART_SERVICES | END_SESSION_INIT_NOTIFY);
	return 0;

fault:
	if (cwmp_create_fault_message(session, rpc, fault_code))
		ret = CWMP_XML_ERR;

	return ret;
}

/*
 * [RPC CPE]: AddObject
 */
int cwmp_handle_rpc_cpe_add_object(struct session *session, struct rpc *rpc)
{
	mxml_node_t *b;
	char *object_name = NULL;
	char *parameter_key = NULL;
	int fault_code = FAULT_CPE_INTERNAL_ERROR, ret = 0;
	char *instance = NULL;

	struct xml_data_struct add_obj_xml_attrs = {0};
	add_obj_xml_attrs.object_name = &object_name;
	add_obj_xml_attrs.parameter_key = &parameter_key;
	struct xml_tag_validation gpn_validation[] = {{"ParameterKey", VALIDATE_STR_SIZE, 0, 32}, {"ObjectName", VALIDATE_STR_SIZE, 0, 256}};
	add_obj_xml_attrs.validations = gpn_validation;
	add_obj_xml_attrs.nbre_validations = 2;

	int fault = load_xml_node_data(SOAP_REQ_ADDOBJ, session->body_in, &add_obj_xml_attrs);

	if (fault) {
		fault_code = fault;
		goto fault;
	}

	if (transaction_id == 0) {
		if (!cwmp_transaction_start("cwmp")) {
			fault_code = FAULT_CPE_INTERNAL_ERROR;
			goto fault;
		}
	}

	if (object_name) {
		char *err = cwmp_add_object(object_name, parameter_key ? parameter_key : "", &instance);
		if (err) {
			fault_code = cwmp_get_fault_code_by_string(err);
			goto fault;
		}
	} else {
		fault_code = FAULT_CPE_INVALID_PARAMETER_NAME;
		goto fault;
	}
	if (instance == NULL) {
		fault_code = FAULT_CPE_INTERNAL_ERROR;
		goto fault;
	}
	b = build_top_body_soap_response(session->tree_out, "AddObject");

	if (!b) {
		fault_code = FAULT_CPE_INTERNAL_ERROR;
		goto fault;
	}

	int instance_int = atoi(instance);
	int status = 1;
	struct xml_data_struct add_resp_xml_attrs = {0};
	add_resp_xml_attrs.instance = &instance_int;
	add_resp_xml_attrs.status = &status;

	fault = build_xml_node_data(SOAP_RESP_ADDOBJ, b, &add_resp_xml_attrs);
	if (fault != CWMP_OK)
		goto fault;

	if (!cwmp_transaction_commit()) {
		fault_code = FAULT_CPE_INTERNAL_ERROR;
		goto fault;
	}

	char *object_path = NULL;
	icwmp_asprintf(&object_path, "%s%s.", object_name, instance);
	cwmp_set_parameter_attributes(object_path, 0);
	FREE(object_name);
	FREE(parameter_key);
	FREE(instance);
	cwmp_set_end_session(END_SESSION_RESTART_SERVICES);
	return 0;

fault:
	FREE(object_name);
	FREE(parameter_key);
	FREE(instance);
	if (cwmp_create_fault_message(session, rpc, fault_code))
		ret = CWMP_XML_ERR;
	if (transaction_id) {
		cwmp_transaction_abort();
		transaction_id = 0;
	}
	return ret;
}

/*
 * [RPC CPE]: DeleteObject
 */
int cwmp_handle_rpc_cpe_delete_object(struct session *session, struct rpc *rpc)
{
	mxml_node_t *b;
	char *object_name = NULL;
	char *parameter_key = NULL;
	int fault_code = FAULT_CPE_INTERNAL_ERROR, ret = 0;

	struct xml_data_struct del_obj_xml_attrs = {0};
	del_obj_xml_attrs.object_name = &object_name;
	del_obj_xml_attrs.parameter_key = &parameter_key;
	struct xml_tag_validation gpn_validation[] = {{"ParameterKey", VALIDATE_STR_SIZE, 0, 32}, {"ObjectName", VALIDATE_STR_SIZE, 0, 256}};
	del_obj_xml_attrs.validations = gpn_validation;
	del_obj_xml_attrs.nbre_validations = 2;

	int fault = load_xml_node_data(SOAP_REQ_DELOBJ, session->body_in, &del_obj_xml_attrs);

	if (fault) {
		fault_code = fault;
		goto fault;
	}

	if (transaction_id == 0) {
		if (!cwmp_transaction_start("cwmp"))
			goto fault;
	}
	if (object_name) {
		char *err = cwmp_delete_object(object_name, parameter_key ? parameter_key : "");
		if (err) {
			fault_code = cwmp_get_fault_code_by_string(err);
			goto fault;
		}
	} else {
		fault_code = FAULT_CPE_INVALID_PARAMETER_NAME;
		goto fault;
	}

	b = build_top_body_soap_response(session->tree_out, "DeleteObject");

	if (!b) {
		fault_code = FAULT_CPE_INTERNAL_ERROR;
		goto fault;
	}

	int status = 1;
	struct xml_data_struct add_resp_xml_attrs = {0};
	add_resp_xml_attrs.status = &status;

	fault = build_xml_node_data(SOAP_RESP_DELOBJ, b, &add_resp_xml_attrs);
	if (fault != CWMP_OK)
		goto fault;

	if (!cwmp_transaction_commit()) {
		fault_code = FAULT_CPE_INTERNAL_ERROR;
		goto fault;
	}
	FREE(object_name);
	FREE(parameter_key);
	cwmp_set_end_session(END_SESSION_RESTART_SERVICES);
	return 0;

fault:
	FREE(object_name);
	FREE(parameter_key);
	if (cwmp_create_fault_message(session, rpc, fault_code))
		ret = CWMP_XML_ERR;
	if (transaction_id) {
		cwmp_transaction_abort();
		transaction_id = 0;
	}
	return ret;
}

/*
 * [RPC CPE]: GetRPCMethods
 */
int cwmp_handle_rpc_cpe_get_rpc_methods(struct session *session, struct rpc *rpc)
{
	mxml_node_t *n, *method_list;
	int i, counter = 0;
	mxml_node_t *b = session->body_in;
	char c[128];

	n = build_top_body_soap_response(session->tree_out, "GetRPCMethods");

	if (!n)
		goto fault;


	LIST_HEAD(rpcs_list);

	for (i = 1; i < __RPC_CPE_MAX; i++) {
		if (i != RPC_CPE_FAULT) {
			struct xml_list_data *xml_data = calloc(1, sizeof(struct xml_list_data));
			xml_data->rpc_name = strdup(rpc_cpe_methods[i].name);
			list_add(&(xml_data->list), &rpcs_list);
			counter++;
		}
	}

	method_list = mxmlNewElement(n, "MethodList");
	if (!method_list)
		goto fault;

	struct xml_data_struct getrpc_resp_xml_attrs = {0};
	getrpc_resp_xml_attrs.data_list = &rpcs_list;

	int fault = build_xml_node_data(SOAP_RESP_GETRPC, method_list, &getrpc_resp_xml_attrs);
	if (fault != CWMP_OK)
		goto fault;

	cwmp_free_all_xml_data_list(&rpcs_list);
	b = mxmlFindElement(session->tree_out, session->tree_out, "MethodList", NULL, NULL, MXML_DESCEND);
	if (!b)
		goto fault;

	mxmlElementSetAttr(b, "xsi:type", "soap_enc:Array");
	if (snprintf(c, sizeof(c), "xsd:string[%d]", counter) == -1)
		goto fault;

	mxmlElementSetAttr(b, "soap_enc:arrayType", c);

	return 0;

fault:
	if (cwmp_create_fault_message(session, rpc, FAULT_CPE_INTERNAL_ERROR))
		goto error;
	return 0;

error:
	return -1;
}

/*
 * [RPC CPE]: FactoryReset
 */
int cwmp_handle_rpc_cpe_factory_reset(struct session *session, struct rpc *rpc)
{
	mxml_node_t *b;

	b = build_top_body_soap_response(session->tree_out, "FactoryReset");

	if (!b)
		goto fault;

	cwmp_set_end_session(END_SESSION_FACTORY_RESET);

	return 0;

fault:
	if (cwmp_create_fault_message(session, rpc, FAULT_CPE_INTERNAL_ERROR))
		goto error;
	return 0;

error:
	return -1;
}

/*
 * [RPC CPE]: X_FactoryResetSoft
 */
int cwmp_handle_rpc_cpe_x_factory_reset_soft(struct session *session, struct rpc *rpc)
{
	mxml_node_t *b;

	b = build_top_body_soap_response(session->tree_out, "X_FactoryResetSoft");

	if (!b)
		goto fault;

	cwmp_set_end_session(END_SESSION_X_FACTORY_RESET_SOFT);

	return 0;

fault:
	if (cwmp_create_fault_message(session, rpc, FAULT_CPE_INTERNAL_ERROR))
		goto error;
	return 0;

error:
	return -1;
}

/*
 * [RPC CPE]: CancelTransfer
 */
int cwmp_handle_rpc_cpe_cancel_transfer(struct session *session, struct rpc *rpc)
{
	mxml_node_t *b;
	char *command_key = NULL;
	int fault_code = FAULT_CPE_INTERNAL_ERROR;
	b = session->body_in;

	struct xml_data_struct canceltrancer_obj_xml_attrs = {0};
	canceltrancer_obj_xml_attrs.command_key = &command_key;
	struct xml_tag_validation canceltransfer_validation[] = {{"CommandKey", VALIDATE_STR_SIZE, 0, 32}};
	canceltrancer_obj_xml_attrs.validations = canceltransfer_validation;
	canceltrancer_obj_xml_attrs.nbre_validations = 1;

	fault_code = load_xml_node_data(SOAP_REQ_CANCELTRANSFER, session->body_in, &canceltrancer_obj_xml_attrs);

	if (command_key)
		cancel_transfer(command_key);

	if (fault_code)
		goto fault;

	b = build_top_body_soap_response(session->tree_out, "CancelTransfer");

	if (!b) {
		fault_code = FAULT_CPE_INTERNAL_ERROR;
		goto fault;
	}
	FREE(command_key);
	return 0;

fault:
	FREE(command_key);
	if (cwmp_create_fault_message(session, rpc, fault_code))
		goto error;
	return 0;

error:
	return -1;
}

int cancel_transfer(char *key)
{
	struct list_head *ilist, *q;

	if (list_download.next != &(list_download)) {
		list_for_each_safe (ilist, q, &(list_download)) {
			struct download *pdownload = list_entry(ilist, struct download, list);
			if (strcmp(pdownload->command_key, key) == 0) {
				pthread_mutex_lock(&mutex_download);
				bkp_session_delete_download(pdownload);
				bkp_session_save();
				list_del(&(pdownload->list));
				if (pdownload->scheduled_time != 0)
					count_download_queue--;
				cwmp_free_download_request(pdownload);
				pthread_mutex_unlock(&mutex_download);
			}
		}
	}
	if (list_upload.next != &(list_upload)) {
		list_for_each_safe (ilist, q, &(list_upload)) {
			struct upload *pupload = list_entry(ilist, struct upload, list);
			if (strcmp(pupload->command_key, key) == 0) {
				pthread_mutex_lock(&mutex_upload);
				bkp_session_delete_upload(pupload);
				bkp_session_save();
				list_del(&(pupload->list));
				if (pupload->scheduled_time != 0)
					count_download_queue--;
				cwmp_free_upload_request(pupload);
				pthread_mutex_unlock(&mutex_upload);
			}
		}
	}
	// Cancel schedule download
	return CWMP_OK;
}

/*
 * [RPC CPE]: Reboot
 */
int cwmp_handle_rpc_cpe_reboot(struct session *session, struct rpc *rpc)
{
	mxml_node_t *b;
	struct event_container *event_container;
	char *command_key = NULL;
	int fault_code = FAULT_CPE_INTERNAL_ERROR;
	b = session->body_in;

	struct xml_data_struct reboot_obj_xml_attrs = {0};
	reboot_obj_xml_attrs.command_key = &command_key;
	struct xml_tag_validation reboot_validation[] = {{"CommandKey", VALIDATE_STR_SIZE, 0, 32}};
	reboot_obj_xml_attrs.validations = reboot_validation;
	reboot_obj_xml_attrs.nbre_validations = 1;

	fault_code = load_xml_node_data(SOAP_REQ_REBOOT, session->body_in, &reboot_obj_xml_attrs);

	if (fault_code)
		goto fault;

	commandKey = icwmp_strdup(command_key);

	pthread_mutex_lock(&(cwmp_main.mutex_session_queue));
	event_container = cwmp_add_event_container(&cwmp_main, EVENT_IDX_M_Reboot, command_key);
	if (event_container == NULL) {
		pthread_mutex_unlock(&(cwmp_main.mutex_session_queue));
		goto fault;
	}
	cwmp_save_event_container(event_container);
	pthread_mutex_unlock(&(cwmp_main.mutex_session_queue));

	b = build_top_body_soap_response(session->tree_out, "Reboot");

	if (!b)
		goto fault;

	cwmp_set_end_session(END_SESSION_REBOOT);

	FREE(command_key);
	return 0;

fault:
FREE(command_key);
	if (cwmp_create_fault_message(session, rpc, fault_code))
		goto error;
	return 0;

error:
	return -1;
}

/*
 * [RPC CPE]: ScheduleInform
 */
int cwmp_handle_rpc_cpe_schedule_inform(struct session *session, struct rpc *rpc)
{
	mxml_node_t *n;
	char *command_key = NULL;
	struct schedule_inform *schedule_inform;
	time_t scheduled_time;
	struct list_head *ilist;
	int fault = FAULT_CPE_NO_FAULT;
	int delay_seconds = 0;


	pthread_mutex_lock(&mutex_schedule_inform);

	struct xml_data_struct schedinform_obj_xml_attrs = {0};
	schedinform_obj_xml_attrs.command_key = &command_key;
	schedinform_obj_xml_attrs.delay_seconds = (long int*)&delay_seconds;
	struct xml_tag_validation schedinform_validation[] = {{"CommandKey", VALIDATE_STR_SIZE, 0, 32}, {"DelaySeconds", VALIDATE_UNINT, 0, 0}};
	schedinform_obj_xml_attrs.validations = schedinform_validation;
	schedinform_obj_xml_attrs.nbre_validations = 2;

	fault = load_xml_node_data(SOAP_REQ_SCHEDINF, session->body_in, &schedinform_obj_xml_attrs);

	FREE(command_key);
	if (fault)
		goto fault;

	if (count_schedule_inform_queue >= MAX_SCHEDULE_INFORM_QUEUE) {
		fault = FAULT_CPE_RESOURCES_EXCEEDED;
		pthread_mutex_unlock(&mutex_schedule_inform);
		goto fault;
	}
	count_schedule_inform_queue++;

	scheduled_time = time(NULL) + delay_seconds;
	list_for_each (ilist, &(list_schedule_inform)) {
		schedule_inform = list_entry(ilist, struct schedule_inform, list);
		if (schedule_inform->scheduled_time >= scheduled_time) {
			break;
		}
	}

	n = build_top_body_soap_response(session->tree_out, "ScheduleInform");

	if (!n)
		goto fault;

	CWMP_LOG(INFO, "Schedule inform event will start in %us", delay_seconds);
	schedule_inform = calloc(1, sizeof(struct schedule_inform));
	if (schedule_inform == NULL) {
		pthread_mutex_unlock(&mutex_schedule_inform);
		goto fault;
	}
	schedule_inform->commandKey = strdup(command_key);
	schedule_inform->scheduled_time = scheduled_time;
	list_add(&(schedule_inform->list), ilist->prev);
	bkp_session_insert_schedule_inform(schedule_inform->scheduled_time, schedule_inform->commandKey);
	bkp_session_save();
	pthread_mutex_unlock(&mutex_schedule_inform);
	pthread_cond_signal(&threshold_schedule_inform);

success:
	return 0;

fault:
	if (cwmp_create_fault_message(session, rpc, fault ? fault : FAULT_CPE_INTERNAL_ERROR))
		goto error;
	goto success;

error:
	return -1;
}

/*
 * [RPC CPE]: ChangeDuState
 */
int cwmp_handle_rpc_cpe_change_du_state(struct session *session, struct rpc *rpc)
{
	mxml_node_t *n, *t;
	struct change_du_state *change_du_state = NULL;
	int error = FAULT_CPE_NO_FAULT;
	char c[256];

	if (snprintf(c, sizeof(c), "%s:%s", ns.cwmp, "ChangeDUState") == -1) {
		error = FAULT_CPE_INTERNAL_ERROR;
		goto fault;
	}

	n = mxmlFindElement(session->tree_in, session->tree_in, c, NULL, NULL, MXML_DESCEND);

	if (!n)
		return -1;

	change_du_state = calloc(1, sizeof(struct change_du_state));
	if (change_du_state == NULL) {
		error = FAULT_CPE_INTERNAL_ERROR;
		goto fault;
	}

	INIT_LIST_HEAD(&(change_du_state->list_operation));
	change_du_state->timeout = time(NULL);

	LIST_HEAD(xml_list_operations);
	struct xml_data_struct cdu_xml_attrs = {0};
	cdu_xml_attrs.command_key = &change_du_state->command_key;
	cdu_xml_attrs.data_list = &xml_list_operations;
	struct xml_tag_validation cdu_validation[] = {{"CommandKey", VALIDATE_STR_SIZE, 0, 32}, {"URL", VALIDATE_STR_SIZE, 0, 1024}, {"UUID", VALIDATE_STR_SIZE, 0, 36}, {"Username", VALIDATE_STR_SIZE, 0, 256}, {"Password", VALIDATE_STR_SIZE, 0, 256}, {"ExecutionEnvRef", VALIDATE_STR_SIZE, 0, 256}, {"Version", VALIDATE_STR_SIZE, 0, 32}};
	cdu_xml_attrs.validations = cdu_validation;
	cdu_xml_attrs.nbre_validations = 7;

	error = load_xml_node_data(SOAP_REQ_CDU, n, &cdu_xml_attrs);

	if (error)
		goto fault;

	xml_data_list_to_cdu_operations_list(&xml_list_operations, &change_du_state->list_operation);

	t = build_top_body_soap_response(session->tree_out, "ChangeDUState");

	if (!t)
		goto fault;

	if (error == FAULT_CPE_NO_FAULT) {
		pthread_mutex_lock(&mutex_change_du_state);
		list_add_tail(&(change_du_state->list), &(list_change_du_state));
		bkp_session_insert_change_du_state(change_du_state);
		bkp_session_save();
		pthread_mutex_unlock(&mutex_change_du_state);
		pthread_cond_signal(&threshold_change_du_state);
	}
	return 0;

fault:
	cwmp_free_change_du_state_request(change_du_state);
	if (cwmp_create_fault_message(session, rpc, error))
		goto error;
	return 0;

error:
	return -1;
}

/*
 * [RPC CPE]: Download
 */
int cwmp_handle_rpc_cpe_download(struct session *session, struct rpc *rpc)
{
	mxml_node_t *n;
	char c[256];
	int error = FAULT_CPE_NO_FAULT;
	struct download *download = NULL, *idownload;
	struct list_head *ilist;
	time_t scheduled_time = 0;
	time_t download_delay = 0;

	if (snprintf(c, sizeof(c), "%s:%s", ns.cwmp, "Download") == -1) {
		error = FAULT_CPE_INTERNAL_ERROR;
		goto fault;
	}

	n = mxmlFindElement(session->tree_in, session->tree_in, c, NULL, NULL, MXML_DESCEND);

	if (!n)
		return -1;

	download = calloc(1, sizeof(struct download));
	if (download == NULL) {
		error = FAULT_CPE_INTERNAL_ERROR;
		goto fault;
	}

	struct xml_data_struct download_xml_attrs = {0};
	download_xml_attrs.command_key = &download->command_key;
	download_xml_attrs.url = &download->url;
	download_xml_attrs.username = &download->username;
	download_xml_attrs.password = &download->password;
	download_xml_attrs.delay_seconds = (long int*)&download_delay;
	download_xml_attrs.file_type = &download->file_type;
	download_xml_attrs.file_size = &download->file_size;

	struct xml_tag_validation download_validation[] = {{"CommandKey", VALIDATE_STR_SIZE, 0, 32}, {"FileType", VALIDATE_STR_SIZE, 0, 64}, {"URL", VALIDATE_STR_SIZE, 0, 256}, {"Username", VALIDATE_STR_SIZE, 0, 256}, {"Password", VALIDATE_STR_SIZE, 0, 256}, {"FileSize", VALIDATE_UNINT, 0, 0}, {"DelaySeconds", VALIDATE_UNINT, 0, 0}};
	download_xml_attrs.validations = download_validation;
	download_xml_attrs.nbre_validations = 7;

	int fault = load_xml_node_data(SOAP_REQ_DOWNLOAD, n, &download_xml_attrs);

	if (fault) {
		error = fault;
		goto fault;
	}

	if (strcmp(download->file_type, FIRMWARE_UPGRADE_IMAGE_FILE_TYPE) && strcmp(download->file_type, WEB_CONTENT_FILE_TYPE) && strcmp(download->file_type, VENDOR_CONFIG_FILE_TYPE) && strcmp(download->file_type, TONE_FILE_TYPE) && strcmp(download->file_type, RINGER_FILE_TYPE) && strcmp(download->file_type, STORED_FIRMWARE_IMAGE_FILE_TYPE)) {
		error = FAULT_CPE_INVALID_ARGUMENTS;
	} else if (count_download_queue >= MAX_DOWNLOAD_QUEUE) {
		error = FAULT_CPE_RESOURCES_EXCEEDED;
	} else if (download->url == NULL || (strcmp(download->url, "") == 0)) {
		error = FAULT_CPE_REQUEST_DENIED;
	} else if (strstr(download->url, "@") != NULL) {
		error = FAULT_CPE_INVALID_ARGUMENTS;
	} else if (strncmp(download->url, DOWNLOAD_PROTOCOL_HTTP, strlen(DOWNLOAD_PROTOCOL_HTTP)) != 0 && strncmp(download->url, DOWNLOAD_PROTOCOL_HTTPS, strlen(DOWNLOAD_PROTOCOL_HTTPS)) != 0 && strncmp(download->url, DOWNLOAD_PROTOCOL_FTP, strlen(DOWNLOAD_PROTOCOL_FTP)) != 0) {
		error = FAULT_CPE_FILE_TRANSFER_UNSUPPORTED_PROTOCOL;
	}
	if (error != FAULT_CPE_NO_FAULT)
		goto fault;

	mxml_node_t *t = build_top_body_soap_response(session->tree_out, "Download");
	char *start_time = "0001-01-01T00:00:00+00:00";
	char *complete_time = "0001-01-01T00:00:00+00:00";
	int status = 1;

	struct xml_data_struct download_resp_xml_attrs = {0};
	download_resp_xml_attrs.status = &status;
	download_resp_xml_attrs.start_time = &start_time;
	download_resp_xml_attrs.complete_time = &complete_time;
	fault = build_xml_node_data(SOAP_RESP_DOWNLOAD, t, &download_resp_xml_attrs);
	if (fault != CWMP_OK) {
		error = FAULT_CPE_INTERNAL_ERROR;
		goto fault;
	}

	if (error == FAULT_CPE_NO_FAULT) {
		pthread_mutex_lock(&mutex_download);
		if (download_delay != 0)
			scheduled_time = time(NULL) + download_delay + PROCESSING_DELAY;

		list_for_each (ilist, &(list_download)) {
			idownload = list_entry(ilist, struct download, list);
			if (idownload->scheduled_time >= scheduled_time) {
				break;
			}
		}
		list_add(&(download->list), ilist->prev);
		if (download_delay != 0) {
			count_download_queue++;
			download->scheduled_time = scheduled_time;
		}
		bkp_session_insert_download(download);
		bkp_session_save();
		if (download_delay != 0) {
			CWMP_LOG(INFO, "Download will start in %us", download_delay);
		} else {
			CWMP_LOG(INFO, "Download will start at the end of session");
		}

		pthread_mutex_unlock(&mutex_download);
		pthread_cond_signal(&threshold_download);
	}

	return 0;

fault:
	cwmp_free_download_request(download);
	if (cwmp_create_fault_message(session, rpc, error))
		return -1;
	return 0;
}

/*
 * [RPC CPE]: ScheduleDownload
 */
int cwmp_handle_rpc_cpe_schedule_download(struct session *session, struct rpc *rpc)
{
	mxml_node_t *n, *t;
	char c[256];
	int i = 0, j = 0;
	int error = FAULT_CPE_NO_FAULT;
	struct download *schedule_download = NULL;
	time_t schedule_download_delay[4] = { 0, 0, 0, 0 };

	if (snprintf(c, sizeof(c), "%s:%s", ns.cwmp, "ScheduleDownload") == -1) {
		error = FAULT_CPE_INTERNAL_ERROR;
		goto fault;
	}

	n = mxmlFindElement(session->tree_in, session->tree_in, c, NULL, NULL, MXML_DESCEND);

	if (!n)
		return -1;

	schedule_download = calloc(1, sizeof(struct download));
	if (schedule_download == NULL) {
		error = FAULT_CPE_INTERNAL_ERROR;
		goto fault;
	}

	struct xml_data_struct sched_download_xml_attrs = {0};
	sched_download_xml_attrs.command_key = &schedule_download->command_key;
	sched_download_xml_attrs.url = &schedule_download->url;
	sched_download_xml_attrs.username = &schedule_download->username;
	sched_download_xml_attrs.password = &schedule_download->password;
	sched_download_xml_attrs.file_type = &schedule_download->file_type;
	sched_download_xml_attrs.file_size = &schedule_download->file_size;

	struct xml_tag_validation scheddownload_validation[] = {{"CommandKey", VALIDATE_STR_SIZE, 0, 32}, {"FileType", VALIDATE_STR_SIZE, 0, 64}, {"URL", VALIDATE_STR_SIZE, 0, 256}, {"Username", VALIDATE_STR_SIZE, 0, 256}, {"Password", VALIDATE_STR_SIZE, 0, 256}, {"FileSize", VALIDATE_UNINT, 0, 0}};
	sched_download_xml_attrs.validations = scheddownload_validation;
	sched_download_xml_attrs.nbre_validations = 6;

	LIST_HEAD(time_window_intervals);
	sched_download_xml_attrs.data_list = &time_window_intervals;

	error = load_xml_node_data(SOAP_REQ_SCHEDDOWN, n, &sched_download_xml_attrs);

	if (error)
		goto fault;

	struct xml_list_data *list_data = NULL;
	if (time_window_intervals.next) {
		list_data = container_of(time_window_intervals.next, struct xml_list_data, list);
		schedule_download->timewindowstruct[0].windowmode = list_data->windowmode;
		schedule_download->timewindowstruct[0].usermessage = list_data->usermessage;
		schedule_download->timewindowstruct[0].maxretries = list_data->max_retries;
		schedule_download->timewindowstruct[0].windowstart = list_data->windowstart;
		schedule_download->timewindowstruct[0].windowend = list_data->windowend;
		if (time_window_intervals.next->next) {
			list_data = container_of(time_window_intervals.next->next, struct xml_list_data, list);
			schedule_download->timewindowstruct[1].windowmode = list_data->windowmode;
			schedule_download->timewindowstruct[1].usermessage = list_data->usermessage;
			schedule_download->timewindowstruct[1].maxretries = list_data->max_retries;
			schedule_download->timewindowstruct[1].windowstart = list_data->windowstart;
			schedule_download->timewindowstruct[1].windowend = list_data->windowend;
		}
	}

	if (strcmp(schedule_download->file_type, FIRMWARE_UPGRADE_IMAGE_FILE_TYPE) && strcmp(schedule_download->file_type, WEB_CONTENT_FILE_TYPE) && strcmp(schedule_download->file_type, VENDOR_CONFIG_FILE_TYPE) && strcmp(schedule_download->file_type, TONE_FILE_TYPE) && strcmp(schedule_download->file_type, RINGER_FILE_TYPE) && strcmp(schedule_download->file_type, STORED_FIRMWARE_IMAGE_FILE_TYPE)) {
		error = FAULT_CPE_INVALID_ARGUMENTS;
	} else if ((strcmp(schedule_download->timewindowstruct[0].windowmode, "1 At Any Time") && strcmp(schedule_download->timewindowstruct[0].windowmode, "2 Immediately") && strcmp(schedule_download->timewindowstruct[0].windowmode, "3 When Idle")) || (strcmp(schedule_download->timewindowstruct[1].windowmode, "1 At Any Time") && strcmp(schedule_download->timewindowstruct[1].windowmode, "2 Immediately") && strcmp(schedule_download->timewindowstruct[1].windowmode, "3 When Idle"))) {
		error = FAULT_CPE_REQUEST_DENIED;
	} else if (count_download_queue >= MAX_DOWNLOAD_QUEUE) {
		error = FAULT_CPE_RESOURCES_EXCEEDED;
	} else if (schedule_download->url == NULL || (strcmp(schedule_download->url, "") == 0)) {
		error = FAULT_CPE_REQUEST_DENIED;
	} else if (strstr(schedule_download->url, "@") != NULL) {
		error = FAULT_CPE_INVALID_ARGUMENTS;
	} else if (strncmp(schedule_download->url, DOWNLOAD_PROTOCOL_HTTP, strlen(DOWNLOAD_PROTOCOL_HTTP)) != 0 && strncmp(schedule_download->url, DOWNLOAD_PROTOCOL_FTP, strlen(DOWNLOAD_PROTOCOL_FTP)) != 0) {
		error = FAULT_CPE_FILE_TRANSFER_UNSUPPORTED_PROTOCOL;
	} else {
		for (j = 0; j < 3; j++) {
			if (schedule_download_delay[j] > schedule_download_delay[j + 1]) {
				error = FAULT_CPE_INVALID_ARGUMENTS;
				break;
			}
		}
	}

	if (error != FAULT_CPE_NO_FAULT)
		goto fault;

	t = build_top_body_soap_response(session->tree_out, "ScheduleDownload");

	if (!t)
		goto fault;

	pthread_mutex_lock(&mutex_schedule_download);
	list_add_tail(&(schedule_download->list), &(list_schedule_download));
	if (schedule_download_delay[0] != 0) {
		count_download_queue++;
	}
	while (i > 0) {
		i--;
		schedule_download->timewindowstruct[i].windowstart = time(NULL) + schedule_download_delay[i * 2];
		schedule_download->timewindowstruct[i].windowend = time(NULL) + schedule_download_delay[i * 2 + 1];
	}
	bkp_session_insert_schedule_download(schedule_download);
	bkp_session_save();
	if (schedule_download_delay[0] != 0) {
		CWMP_LOG(INFO, "Schedule download will start in %us", schedule_download_delay[0]);
	} else {
		CWMP_LOG(INFO, "Schedule Download will start at the end of session");
	}
	pthread_mutex_unlock(&mutex_schedule_download);
	pthread_cond_signal(&threshold_schedule_download);

	return 0;

fault:
	cwmp_free_schedule_download_request(schedule_download);
	if (cwmp_create_fault_message(session, rpc, error))
		goto error;
	return 0;

error:
	return -1;
}

/*
 * [RPC CPE]: Upload
 */
int cwmp_handle_rpc_cpe_upload(struct session *session, struct rpc *rpc)
{
	mxml_node_t *n;
	int error = FAULT_CPE_NO_FAULT;
	struct upload *upload = NULL, *iupload;
	struct list_head *ilist;
	time_t scheduled_time = 0;
	time_t upload_delay = 0;
	char c[256];

	if (snprintf(c, sizeof(c), "%s:%s", ns.cwmp, "Upload") == -1) {
		error = FAULT_CPE_INTERNAL_ERROR;
		goto fault;
	}

	n = mxmlFindElement(session->tree_in, session->tree_in, c, NULL, NULL, MXML_DESCEND);

	if (!n)
		return -1;

	upload = calloc(1, sizeof(struct upload));
	if (upload == NULL) {
		error = FAULT_CPE_INTERNAL_ERROR;
		goto fault;
	}
	upload->f_instance = 0;

	struct xml_data_struct upload_xml_attrs = {0};
	upload_xml_attrs.command_key = &upload->command_key;
	upload_xml_attrs.url = &upload->url;
	upload_xml_attrs.username = &upload->username;
	upload_xml_attrs.password = &upload->password;
	upload_xml_attrs.delay_seconds = (long int*)&upload_delay;
	upload_xml_attrs.file_type = &upload->file_type;
	upload_xml_attrs.instance = &upload->f_instance;


	struct xml_tag_validation upload_validation[] = {{"CommandKey", VALIDATE_STR_SIZE, 0, 32}, {"FileType", VALIDATE_STR_SIZE, 0, 64}, {"URL", VALIDATE_STR_SIZE, 0, 256}, {"Username", VALIDATE_STR_SIZE, 0, 256}, {"Password", VALIDATE_STR_SIZE, 0, 256}, {"DelaySeconds", VALIDATE_UNINT, 0, 0}};
	upload_xml_attrs.validations = upload_validation;
	upload_xml_attrs.nbre_validations = 6;

	error = load_xml_node_data(SOAP_REQ_UPLOAD, n, &upload_xml_attrs);

	if (error)
		goto fault;

	if (count_download_queue >= MAX_DOWNLOAD_QUEUE) {
		error = FAULT_CPE_RESOURCES_EXCEEDED;
	} else if (upload->url == NULL || (strcmp(upload->url, "") == 0)) {
		error = FAULT_CPE_REQUEST_DENIED;
	} else if (strstr(upload->url, "@") != NULL) {
		error = FAULT_CPE_INVALID_ARGUMENTS;
	} else if (strncmp(upload->url, DOWNLOAD_PROTOCOL_HTTPS, strlen(DOWNLOAD_PROTOCOL_HTTPS)) != 0  && strncmp(upload->url, DOWNLOAD_PROTOCOL_HTTP, strlen(DOWNLOAD_PROTOCOL_HTTP)) != 0 && strncmp(upload->url, DOWNLOAD_PROTOCOL_FTP, strlen(DOWNLOAD_PROTOCOL_FTP)) != 0) {
		error = FAULT_CPE_FILE_TRANSFER_UNSUPPORTED_PROTOCOL;
	}

	if (error != FAULT_CPE_NO_FAULT) {
		goto fault;
	}

	mxml_node_t *t = build_top_body_soap_response(session->tree_out, "Upload");
	char *start_time = "0001-01-01T00:00:00+00:00";
	char *complete_time = "0001-01-01T00:00:00+00:00";
	int status = 1;

	struct xml_data_struct upload_resp_xml_attrs = {0};
	upload_resp_xml_attrs.status = &status;
	upload_resp_xml_attrs.start_time = &start_time;
	upload_resp_xml_attrs.complete_time = &complete_time;
	int fault = build_xml_node_data(SOAP_RESP_UPLOAD, t, &upload_resp_xml_attrs);
	if (fault != CWMP_OK) {
		error = FAULT_CPE_INTERNAL_ERROR;
		goto fault;
	}

	if (error == FAULT_CPE_NO_FAULT) {
		pthread_mutex_lock(&mutex_upload);
		if (upload_delay != 0)
			scheduled_time = time(NULL) + upload_delay + PROCESSING_DELAY;

		list_for_each (ilist, &(list_upload)) {
			iupload = list_entry(ilist, struct upload, list);
			if (iupload->scheduled_time >= scheduled_time) {
				break;
			}
		}
		list_add(&(upload->list), ilist->prev);
		if (upload_delay != 0) {
			count_download_queue++;
			upload->scheduled_time = scheduled_time;
		}
		bkp_session_insert_upload(upload);
		bkp_session_save();
		if (upload_delay != 0) {
			CWMP_LOG(INFO, "Upload will start in %us", upload_delay);
		} else {
			CWMP_LOG(INFO, "Upload will start at the end of session");
		}
		pthread_mutex_unlock(&mutex_upload);
		pthread_cond_signal(&threshold_upload);
	}
	return 0;

fault:
	cwmp_free_upload_request(upload);
	if (cwmp_create_fault_message(session, rpc, error))
		return -1;
	return 0;
}

/*
 * [FAULT]: Fault
 */

int cwmp_handle_rpc_cpe_fault(struct session *session, struct rpc *rpc)
{
	mxml_node_t *body;

	body = mxmlFindElement(session->tree_out, session->tree_out, "soap_env:Body", NULL, NULL, MXML_DESCEND);
	struct xml_data_struct fault_xml_attrs = {0};
	char *faultcode = (FAULT_CPE_ARRAY[session->fault_code].TYPE == FAULT_CPE_TYPE_CLIENT) ? "Client" : "Server";
	char *faultstring = "CWMP fault";
	int fault_code = atoi(session->fault_code ? FAULT_CPE_ARRAY[session->fault_code].CODE : "0");
	char *fault_string = strdup(FAULT_CPE_ARRAY[session->fault_code].DESCRIPTION);
	fault_xml_attrs.fault_code = &fault_code;
	fault_xml_attrs.fault_string = &fault_string;
	fault_xml_attrs.faultcode = &faultcode;
	fault_xml_attrs.faultstring = &faultstring;

	int fault = build_xml_node_data(SOAP_ROOT_FAULT, body, &fault_xml_attrs);
	FREE(fault_string);
	if (fault)
		return -1;

	if (rpc->type == RPC_CPE_SET_PARAMETER_VALUES) {
		LIST_HEAD(spv_fault_xml_data_list);
		cwmp_param_fault_list_to_xml_data_list(rpc->list_set_value_fault, &spv_fault_xml_data_list);
		struct xml_data_struct spv_fault_xml_attrs = {0};
		spv_fault_xml_attrs.data_list = &spv_fault_xml_data_list;
		body = mxmlFindElement(session->tree_out, session->tree_out, "cwmp:Fault", NULL, NULL, MXML_DESCEND);
		fault = build_xml_node_data(SOAP_SPV_FAULT, body, &spv_fault_xml_attrs);
		if (fault)
			return -1;
		cwmp_free_all_xml_data_list(&spv_fault_xml_data_list);
	}

	return 0;
}

int cwmp_create_fault_message(struct session *session, struct rpc *rpc_cpe, int fault_code)
{
	CWMP_LOG(INFO, "Fault detected");
	session->fault_code = fault_code;

	MXML_DELETE(session->tree_out);

	if (xml_prepare_msg_out(session))
		return -1;

	CWMP_LOG(INFO, "Preparing the Fault message");
	if (rpc_cpe_methods[RPC_CPE_FAULT].handler(session, rpc_cpe))
		return -1;
	rpc_cpe->type = RPC_CPE_FAULT;

	return 0;
}
