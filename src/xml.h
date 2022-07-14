#ifndef __XML__H_
#define __XML__H_

#include "xml_utils.h"
#include "session.h"
#include "common.h"

#define CWMP_MXML_TAB_SPACE "  "
#define MAX_SCHEDULE_INFORM_QUEUE 10

#define XML2(A,B) {A, B, 0, NULL}
#define XML3(A,B,C) {A, B, C, NULL}
#define XML4(A,B,D) {A, B, 0, D}
enum soap_methods {
	SOAP_REQ_SPV = 1,
	SOAP_REQ_GPV,
	SOAP_REQ_GPN,
	SOAP_REQ_SPA,
	SOAP_REQ_GPA,
	SOAP_REQ_ADDOBJ,
	SOAP_REQ_DELOBJ,
	SOAP_REQ_REBOOT,
	SOAP_REQ_DOWNLOAD,
	SOAP_REQ_UPLOAD,
	SOAP_REQ_CANCELTRANSFER,
	SOAP_REQ_SCHEDINF,
	SOAP_REQ_SCHEDDOWN,
	SOAP_REQ_CDU,
	SOAP_REQ_SPV_LIST,
	SOAP_REQ_SPV_LIST_REF,
	SOAP_REQ_GPV_REF,
	SOAP_REQ_SPA_REF,
	SOAP_REQ_GPA_REF,
	SOAP_TIMEWINDOW_REF,
	SOAP_TIME_REF,
	SOAP_REQ_CDU_OPERATIONS,
	SOAP_REQ_CDU_OPS_REF,
	SOAP_REQ_DU_INSTALL,
	SOAP_REQ_DU_UPDATE,
	SOAP_REQ_DU_UNINSTALL,

	SOAP_RESP_GPV,
	SOAP_PARAM_STRUCT,
	SOAP_PARAM_STRUCT_REF,
	SOAP_VALUE_STRUCT,
	SOAP_RESP_SPV,
	SOAP_RESP_GPN,
	SOAP_RESP_GPN_REF,
	SOAP_RESP_GPA,
	SOAP_RESP_GPA_REF,
	SOAP_RESP_ADDOBJ,
	SOAP_RESP_DELOBJ,
	SOAP_RESP_DOWNLOAD,
	SOAP_RESP_UPLOAD,
	SOAP_RESP_GETRPC,
	SOAP_RESP_GETRPC_REF,
	SOAP_ACS_TRANSCOMPLETE,
	SOAP_ROOT_FAULT,
	SOAP_RPC_FAULT,
	SOAP_FAULT_DETAIL,
	SOAP_CWMP_FAULT,
	SOAP_SPV_FAULT,
	SOAP_SPV_FAULT_REF,
	SOAP_FAULT_STRCT_REF,
	SOAP_ENV,
	SOAP_HEAD,
	SOAP_BODY,
	SOAP_INFORM_CWMP,
	SOAP_DEVID,
	SOAP_DU_CHANGE_COMPLETE,
	SOAP_CDU_RESULTS_REF,
	SOAP_CDU_OPTS_REF,
	ATTR_PARAM_STRUCT,
	ATTR_SOAP_ENV,
	SOAP_MAX
};

enum xml_tag_types {
	XML_STRING,
	XML_BOOL,
	XML_INTEGER,
	XML_LINTEGER,
	XML_TIME,
	XML_FUNC,
	XML_REC,
	XML_NODE,
	XML_ATTR
};

enum tag_multiple_single {
	XML_SINGLE,
	XML_LIST
};

enum validation_types {
	VALIDATE_STR_SIZE,
	VALIDATE_UNINT,
	VALIDATE_BOOLEAN,
	VALIDATE_INT_RANGE
};

struct xml_tag_validation {
	char *tag_name;
	int validation_type;
	int min;
	int max;
};

struct xml_data_struct {
	mxml_node_t **parameter_list;
	char **name;
	char **value;
	char **string;
	char **parameter_path;
	char **parameter_name;
	char **object_name;
	char **parameter_key;
	char **command_key;
	char **file_type;
	char **url;
	char **username;
	char **password;
	char **uuid;
	char **exec_env_ref;
	char **du_ref;
	char **current_state;
	char **version;
	char **window_mode;
	char **user_message;
	char **start_time;
	char **complete_time;
	char **access_list;
	char **fault_string;
	char **faultcode;
	char **faultstring;
	char **manufacturer;
	char **oui;
	char **serial_number;
	char **current_time;
	char **product_class;
	char **xsi_type;
	int *file_size;
	int *notification;
	int *scheddown_max_retries;
	int *status;
	int *instance;
	int *fault_code;
	int *max_envelopes;
	int *retry_count;
	long int *delay_seconds;
	long int *window_start;
	long int *window_end;
	bool *next_level;
	bool *notification_change;
	bool *writable;

	mxml_node_t **xml_env;
	struct list_head *data_list;
	char **xcwmp;
	int *amd_version;
	unsigned int *session_timeout;
	int *cdu_type;
	int *counter;
	struct xml_tag_validation *validations;
	int nbre_validations;
};

struct xml_list_data {
	struct list_head list;
	char *param_name;
	char *param_value;
	char *param_type;
	char *windowmode;
	char *usermessage;
	char *access_list;
	char *rpc_name;
	char *fault_string;
	char *url;
	char *uuid;
	char *username;
	char *password;
	char *execution_env_ref;
	char *version;
	char *du_ref;
	char *current_state;
	char *start_time;
	char *complete_time;
	char *command_key;

	long int windowstart;
	long int windowend;
	int max_retries;
	int notification;
	int fault_code;
	int cdu_type;
	int event_code;
	bool notification_change;
	bool writable;
};

struct xml_tag {
	char *tag_name;
	int tag_type;
	int rec_ref;
	int (*xml_func)(mxml_node_t *node, struct xml_data_struct *xml_attrs);
};

struct xml_node_data {
	int node_ms;
	int tag_node_ref;
	char *tag_list_name;
	struct xml_tag xml_tags[10];
};

#define MXML_DELETE(X)                                                                                                                                                                                                                                                                                     \
	do {                                                                                                                                                                                                                                                                                               \
		if (X) {                                                                                                                                                                                                                                                                                   \
			mxmlDelete(X);                                                                                                                                                                                                                                                                     \
			X = NULL;                                                                                                                                                                                                                                                                          \
		}                                                                                                                                                                                                                                                                                          \
	} while (0)

extern const char *cwmp_urls[];
int xml_prepare_msg_out(struct session *session);
int xml_prepare_lwnotification_message(char **msg_out);
int xml_set_cwmp_id_rpc_cpe(struct session *session);
int xml_recreate_namespace(mxml_node_t *tree);
const char *whitespace_cb(mxml_node_t *node, int where);
int xml_set_cwmp_id(struct session *session);
int xml_send_message(struct cwmp *cwmp, struct session *session, struct rpc *rpc);
mxml_node_t *mxmlFindElementOpaque(mxml_node_t *node, mxml_node_t *top, const char *text, int descend);
char *xml__get_attribute_name_by_value(mxml_node_t *node,	const char  *value);
char *xml_get_cwmp_version(int version);
void xml_exit(void);
void load_response_xml_schema(mxml_node_t **schema);
void load_notification_xml_schema(mxml_node_t **tree);
int load_xml_node_data(int node_ref, mxml_node_t *node, struct xml_data_struct *xml_attrs);
int build_xml_node_data(int node_ref, mxml_node_t *node, struct xml_data_struct *xml_attrs);
void add_xml_data_list(struct list_head *data_list, struct xml_list_data *xml_data);
mxml_node_t * build_top_body_soap_response(mxml_node_t *node, char *method);
mxml_node_t * build_top_body_soap_request(mxml_node_t *node, char *method);
void dm_parameter_list_to_xml_data_list(struct list_head *dm_parameter_list, struct list_head *xml_data_list);
void xml_data_list_to_dm_parameter_list(struct list_head *xml_data_list, struct list_head *dm_parameter_list);
void xml_data_list_to_cdu_operations_list(struct list_head *xml_data_list, struct list_head *du_op_list);
void cdu_operations_list_to_xml_data_list(struct list_head *du_op_list, struct list_head *xml_data_list);
void event_container_list_to_xml_data_list(struct list_head *event_container, struct list_head *xml_data_list);
void cwmp_param_fault_list_to_xml_data_list(struct list_head *param_fault_list, struct list_head *xml_data_list);
void cwmp_free_all_xml_data_list(struct list_head *list);
int load_upload_filetype(mxml_node_t *b, struct xml_data_struct *xml_attrs);
int load_download_filetype(mxml_node_t *b, struct xml_data_struct *xml_attrs);
int load_sched_download_window_mode(mxml_node_t *b, struct xml_data_struct *xml_attrs);
int load_change_du_state_operation(mxml_node_t *b, struct xml_data_struct *xml_attrs);
int build_inform_events(mxml_node_t *b, struct xml_data_struct *xml_attrs);
int build_inform_env_header(mxml_node_t *b, struct xml_data_struct *xml_attrs);
#endif
