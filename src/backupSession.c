/*
 * backupSession.c - API to store/load CWMP session in/from backup XML files
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

#include <unistd.h>

#include "backupSession.h"
#include "log.h"
#include "event.h"
#include "sched_inform.h"
#include "download.h"
#include "upload.h"
#include "cwmp_du_state.h"
#include "notifications.h"
#include "xml.h"
#include "cwmp_event.h"

static mxml_node_t *bkp_tree = NULL;

enum backup_attributes_types
{
	BKP_STRING,
	BKP_INTEGER,
	BKP_BOOL,
	BKP_TIME,
};

struct backup_attributes_name_type {
	char *name;
	enum backup_attributes_types bkp_type;
};

struct backup_attributes_name_type bkp_attrs_names[] = { { "command_key", BKP_STRING },
							 { "url", BKP_STRING },
							 { "file_type", BKP_STRING },
							 { "username", BKP_STRING },
							 { "password", BKP_STRING },
							 { "windowmode1", BKP_STRING },
							 { "usermessage1", BKP_STRING },
							 { "windowmode2", BKP_STRING },
							 { "usermessage2", BKP_STRING },
							 { "start_time", BKP_STRING },
							 { "complete_time", BKP_STRING },
							 { "uuid", BKP_STRING },
							 { "version", BKP_STRING },
							 { "du_ref", BKP_STRING },
							 { "current_state", BKP_STRING },
							 { "execution_unit_ref", BKP_STRING },
							 { "old_software_version", BKP_STRING },
							 { "executionenvref", BKP_STRING },
							 { "index", BKP_INTEGER },
							 { "id", BKP_INTEGER },
							 { "file_size", BKP_INTEGER },
							 { "maxretrie1", BKP_INTEGER },
							 { "maxretrie2", BKP_INTEGER },
							 { "type", BKP_INTEGER },
							 { "fault", BKP_INTEGER },
							 { "fault_code", BKP_INTEGER },
							 { "resolved", BKP_BOOL },
							 { "time", BKP_TIME },
							 { "fault_string", BKP_STRING },
							 { "operation", BKP_STRING },
							 { "windowstart1", BKP_TIME },
							 { "windowend1", BKP_TIME },
							 { "windowstart2", BKP_TIME },
							 { "windowend2", BKP_TIME } };
struct backup_attributes {
	char **command_key;
	char **url;
	char **file_type;
	char **username;
	char **password;
	char **windowmode1;
	char **usermessage1;
	char **windowmode2;
	char **usermessage2;
	char **start_time;
	char **complete_time;
	char **uuid;
	char **version;
	char **du_ref;
	char **current_state;
	char **execution_unit_ref;
	char **old_software_version;
	char **executionenvref;
	int *index;
	int *id;
	int *file_size;
	int *maxretrie1;
	int *maxretrie2;
	int *type;
	int *fault;
	int *fault_code;
	bool *resolved;
	time_t *time;
	char **fault_string;
	char **operation;
	time_t *windowstart1;
	time_t *windowend1;
	time_t *windowstart2;
	time_t *windowend2;
};

int get_bkp_attribute_index_type(const char *name)
{
	unsigned int i;
	if (name == NULL)
		return -1;
	size_t total_size = sizeof(bkp_attrs_names) / sizeof(struct backup_attributes_name_type);
	for (i = 0; i < total_size; i++) {
		if (strcmp(name, bkp_attrs_names[i].name) == 0)
			return i;
	}
	return -1;
}

void load_specific_backup_attributes(mxml_node_t *tree, struct backup_attributes *bkp_attrs)
{
	mxml_node_t *b = tree, *c;
	int idx;
	void **ptr;

	b = mxmlWalkNext(b, tree, MXML_DESCEND);
	while (b) {
		if (mxmlGetType(b) == MXML_ELEMENT) {
			idx = get_bkp_attribute_index_type(mxmlGetElement(b));
			if (idx == -1) {
				b = mxmlWalkNext(b, tree, MXML_NO_DESCEND);
				continue;
			}
			c = mxmlWalkNext(b, b, MXML_DESCEND);
			if (c && mxmlGetType(c) == MXML_OPAQUE) {
				const char *opaque = mxmlGetOpaque(c);
				if (opaque != NULL) {
					char **str;
					int *intgr;
					bool *bol;
					time_t *time;

					ptr = (void **)((char *)bkp_attrs + idx * sizeof(char *));
					if (ptr == NULL || *ptr == NULL) {
						b = mxmlWalkNext(b, tree, MXML_NO_DESCEND);
						continue;
					}
					switch (bkp_attrs_names[idx].bkp_type) {
					case BKP_STRING:
						str = (char **)(*ptr);
						*str = strdup(opaque);
						break;
					case BKP_INTEGER:
						intgr = (int *)(*ptr);
						*intgr = atoi(opaque);
						break;
					case BKP_BOOL:
						bol = (bool *)(*ptr);
						*bol = opaque;
						break;
					case BKP_TIME:
						time = (time_t *)(*ptr);
						*time = atol(opaque);
						break;
					}
				}
			}
		}
		b = mxmlWalkNext(b, tree, MXML_NO_DESCEND);
	}
}

void bkp_tree_clean(void)
{
	if (bkp_tree != NULL)
		MXML_DELETE(bkp_tree);
	return;
}

void bkp_session_save()
{
	FILE *fp;
	if (!bkp_tree)
		return;
	fp = fopen(CWMP_BKP_FILE, "w");
	mxmlSaveFile(bkp_tree, fp, MXML_NO_CALLBACK);
	fclose(fp);
	sync();
}

mxml_node_t *bkp_session_insert(mxml_node_t *tree, char *name, char *value)
{
	mxml_node_t *b;
	if (tree == NULL || name == NULL) {
		CWMP_LOG(ERROR, "backup %s: tree or name is null: %p %p", __FUNCTION__, tree, name);
		return NULL;
	}
	b = mxmlNewElement(tree, name);
	if (b == NULL)
		return NULL;

	if (value != NULL)
		mxmlNewOpaque(b, value);

	return b;
}
/*
 * The order of key array filling should be the same of insertion function
 */
mxml_node_t *bkp_session_node_found(mxml_node_t *tree, char *name, struct search_keywords *keys, int size)
{
	mxml_node_t *b = tree, *c, *d;
	struct search_keywords;
	int i = 0;
	if (!tree)
		return NULL;
	b = mxmlFindElement(b, b, name, NULL, NULL, MXML_DESCEND_FIRST);
	while (b) {
		c = mxmlGetFirstChild(b);
		if (c) {
			i = 0;
			while (c && i < size) {
				if (mxmlGetType(c) == MXML_ELEMENT && strcmp(keys[i].name, (char *) mxmlGetElement(c)) == 0) {
					d = c;
					d = mxmlWalkNext(d, c, MXML_DESCEND);
					if ((keys[i].value == NULL) || (d && mxmlGetType(d) == MXML_OPAQUE && CWMP_STRCMP(keys[i].value, mxmlGetOpaque(d)) == 0))
						i++;
				}
				c = mxmlWalkNext(c, b, MXML_NO_DESCEND);
			}
		}
		if (i == size)
			break;

		b = mxmlWalkNext(b, tree, MXML_NO_DESCEND);
	}
	return b;
}

mxml_node_t *bkp_session_insert_event(int index, char *command_key, int id)
{
	struct search_keywords keys[1];
	char event_id[32];
	char event_idx[32];
	mxml_node_t *b;

	snprintf(event_id, sizeof(event_id), "%d", id);
	snprintf(event_idx, sizeof(event_idx), "%d", index);
	keys[0].name = "id";
	keys[0].value = event_id;
	b = bkp_session_node_found(bkp_tree, "cwmp_event", keys, 1);
	if (!b) {
		b = bkp_session_insert(bkp_tree, "cwmp_event", NULL);
		bkp_session_insert(b, "index", event_idx);
		bkp_session_insert(b, "id", event_id);
		bkp_session_insert(b, "command_key", command_key ? command_key : "");
	}
	return b;
}

void bkp_session_delete_event(int id)
{
	struct search_keywords keys[1];
	char event_id[32];
	mxml_node_t *b;

	snprintf(event_id, sizeof(event_id), "%d", id);
	keys[0].name = "id";
	keys[0].value = event_id;
	b = bkp_session_node_found(bkp_tree, "cwmp_event", keys, 1);
	if (b)
		mxmlDelete(b);
}

void bkp_session_insert_parameter(mxml_node_t *b, char *name)
{
	bkp_session_insert(b, "parameter", name);
}

void bkp_session_simple_insert(char *parent, char *child, char *value)
{
	mxml_node_t *b = bkp_tree;

	if (parent == NULL || child == NULL) {
		CWMP_LOG(ERROR, "backup %s: parent or child is null %p %p", __FUNCTION__, parent, child);
		return;
	}
	b = mxmlFindElement(b, b, parent, NULL, NULL, MXML_DESCEND);
	if (b)
		mxmlDelete(b);
	b = bkp_session_insert(bkp_tree, parent, NULL);
	bkp_session_insert(b, child, value);
}

void bkp_session_simple_insert_in_parent(char *parent, char *child, char *value)
{
	mxml_node_t *n, *b = bkp_tree;

	if (parent == NULL || child == NULL) {
		CWMP_LOG(ERROR, "backup %s: parent or child is null %p %p", __FUNCTION__, parent, child);
		return;
	}
	n = mxmlFindElement(b, b, parent, NULL, NULL, MXML_DESCEND);
	if (!n)
		n = bkp_session_insert(bkp_tree, parent, NULL);
	b = mxmlFindElement(n, n, child, NULL, NULL, MXML_DESCEND);
	if (b)
		mxmlDelete(b);
	bkp_session_insert(n, child, value);
}

void bkp_session_insert_schedule_inform(time_t time, char *command_key)
{
	char schedule_time[128];
	mxml_node_t *b;

	snprintf(schedule_time, sizeof(schedule_time), "%lld", (long long int)time);
	struct search_keywords sched_inf_insert_keys[2] = { { "command_key", command_key }, { "time", schedule_time } };
	b = bkp_session_node_found(bkp_tree, "schedule_inform", sched_inf_insert_keys, 2);
	if (!b) {
		b = bkp_session_insert(bkp_tree, "schedule_inform", NULL);
		bkp_session_insert(b, "command_key", command_key ? command_key : "");
		bkp_session_insert(b, "time", schedule_time);
	}
}

void bkp_session_delete_schedule_inform(time_t time, char *command_key)
{
	char schedule_time[128];
	mxml_node_t *b;

	snprintf(schedule_time, sizeof(schedule_time), "%lld", (long long int)time);
	struct search_keywords sched_inf_del_keys[2] = { { "command_key", command_key }, { "time", schedule_time } };
	b = bkp_session_node_found(bkp_tree, "schedule_inform", sched_inf_del_keys, 2);
	if (b)
		mxmlDelete(b);
}

void bkp_session_insert_download(struct download *pdownload)
{
	char schedule_time[128];
	char file_size[128];
	mxml_node_t *b;

	snprintf(schedule_time, sizeof(schedule_time), "%lld", (long long int)pdownload->scheduled_time);
	snprintf(file_size, sizeof(file_size), "%d", pdownload->file_size);
	struct search_keywords download_insert_keys[7] = { { "url", pdownload->url }, { "command_key", pdownload->command_key }, { "file_type", pdownload->file_type }, { "username", pdownload->username }, { "password", pdownload->password }, { "file_size", file_size }, { "time", schedule_time } };

	b = bkp_session_node_found(bkp_tree, "download", download_insert_keys, 7);
	if (!b) {
		b = bkp_session_insert(bkp_tree, "download", NULL);
		bkp_session_insert(b, "url", pdownload->url ? pdownload->url : "");
		bkp_session_insert(b, "command_key", pdownload->command_key ? pdownload->command_key : "");
		bkp_session_insert(b, "file_type", pdownload->file_type ? pdownload->file_type : "");
		bkp_session_insert(b, "username", pdownload->username ? pdownload->username : "");
		bkp_session_insert(b, "password", pdownload->password ? pdownload->password : "");
		bkp_session_insert(b, "file_size", file_size);
		bkp_session_insert(b, "time", schedule_time);
	}
}

void bkp_session_insert_schedule_download(struct download *pschedule_download)
{
	char delay[4][128];
	int i;
	char file_size[128];
	char maxretrie[2][128];
	mxml_node_t *b;

	snprintf(file_size, sizeof(file_size), "%d", pschedule_download->file_size);
	for (i = 0; i < 2; i++) {
		snprintf(delay[2 * i], sizeof(delay[i]), "%lld", (long long int)pschedule_download->timewindowstruct[i].windowstart);
		snprintf(delay[2 * i + 1], sizeof(delay[i]), "%lld", (long long int)pschedule_download->timewindowstruct[i].windowend);
		snprintf(maxretrie[i], sizeof(maxretrie[i]), "%d", pschedule_download->timewindowstruct[i].maxretries);
	}
	struct search_keywords sched_download_insert_keys[16] = { { "url", pschedule_download->url },
								  { "command_key", pschedule_download->command_key },
								  { "file_type", pschedule_download->file_type },
								  { "username", pschedule_download->username },
								  { "password", pschedule_download->password },
								  { "file_size", file_size },
								  { "windowstart1", delay[0] },
								  { "windowend1", delay[1] },
								  { "windowmode1", pschedule_download->timewindowstruct[0].windowmode },
								  { "usermessage1", pschedule_download->timewindowstruct[0].usermessage },
								  { "maxretrie1", maxretrie[0] },
								  { "windowstart2", delay[2] },
								  { "windowend2", delay[3] },
								  { "windowmode2", pschedule_download->timewindowstruct[1].windowmode },
								  { "usermessage2", pschedule_download->timewindowstruct[1].usermessage },
								  { "maxretrie2", maxretrie[1] } };

	b = bkp_session_node_found(bkp_tree, "schedule_download", sched_download_insert_keys, 16);
	if (!b) {
		b = bkp_session_insert(bkp_tree, "schedule_download", NULL);
		bkp_session_insert(b, "url", pschedule_download->url);
		bkp_session_insert(b, "command_key", pschedule_download->command_key);
		bkp_session_insert(b, "file_type", pschedule_download->file_type);
		bkp_session_insert(b, "username", pschedule_download->username);
		bkp_session_insert(b, "password", pschedule_download->password);
		bkp_session_insert(b, "file_size", file_size);
		bkp_session_insert(b, "windowstart1", delay[0]);
		bkp_session_insert(b, "windowend1", delay[1]);
		bkp_session_insert(b, "windowmode1", pschedule_download->timewindowstruct[0].windowmode);
		bkp_session_insert(b, "usermessage1", pschedule_download->timewindowstruct[0].usermessage);
		bkp_session_insert(b, "maxretrie1", maxretrie[0]);
		bkp_session_insert(b, "windowstart2", delay[2]);
		bkp_session_insert(b, "windowend2", delay[3]);
		bkp_session_insert(b, "windowmode2", pschedule_download->timewindowstruct[1].windowmode);
		bkp_session_insert(b, "usermessage2", pschedule_download->timewindowstruct[1].usermessage);
		bkp_session_insert(b, "maxretrie2", maxretrie[1]);
	}
}

void bkp_session_insert_change_du_state(struct change_du_state *pchange_du_state)
{
	struct operations *p = NULL;
	char schedule_time[128];
	mxml_node_t *b, *n;

	snprintf(schedule_time, sizeof(schedule_time), "%lld", (long long int)pchange_du_state->timeout);
	b = bkp_session_insert(bkp_tree, "change_du_state", NULL);
	bkp_session_insert(b, "command_key", pchange_du_state->command_key);
	bkp_session_insert(b, "time", schedule_time);
	list_for_each_entry (p, &(pchange_du_state->list_operation), list) {
		if (p->type == DU_INSTALL) {
			n = bkp_session_insert(b, "install", NULL);
			bkp_session_insert(n, "url", p->url);
			bkp_session_insert(n, "uuid", p->uuid);
			bkp_session_insert(n, "username", p->username);
			bkp_session_insert(n, "password", p->password);
			bkp_session_insert(n, "executionenvref", p->executionenvref);
		} else if (p->type == DU_UPDATE) {
			n = bkp_session_insert(b, "update", NULL);
			bkp_session_insert(n, "uuid", p->uuid);
			bkp_session_insert(n, "version", p->version);
			bkp_session_insert(n, "url", p->url);
			bkp_session_insert(n, "username", p->username);
			bkp_session_insert(n, "password", p->password);
		} else if (p->type == DU_UNINSTALL) {
			n = bkp_session_insert(b, "uninstall", NULL);
			bkp_session_insert(n, "uuid", p->uuid);
			bkp_session_insert(n, "version", p->version);
			bkp_session_insert(n, "executionenvref", p->executionenvref);
		}
	}
}

void bkp_session_delete_change_du_state(struct change_du_state *pchange_du_state)
{
	char schedule_time[128];
	mxml_node_t *b;

	snprintf(schedule_time, sizeof(schedule_time), "%lld", (long long int)pchange_du_state->timeout);
	struct search_keywords cds_del_keys[2] = { { "command_key", pchange_du_state->command_key }, { "time", schedule_time } };
	b = bkp_session_node_found(bkp_tree, "change_du_state", cds_del_keys, 2);
	if (b)
		mxmlDelete(b);
}

void bkp_session_insert_upload(struct upload *pupload)
{
	char schedule_time[128];
	mxml_node_t *b;

	snprintf(schedule_time, sizeof(schedule_time), "%lld", (long long int)pupload->scheduled_time);
	struct search_keywords upload_insert_keys[6] = { { "url", pupload->url }, { "command_key", pupload->command_key }, { "username", pupload->username }, { "password", pupload->password }, { "time", schedule_time }, { "file_type", pupload->file_type } };

	b = bkp_session_node_found(bkp_tree, "upload", upload_insert_keys, 6);
	if (!b) {
		b = bkp_session_insert(bkp_tree, "upload", NULL);
		bkp_session_insert(b, "url", pupload->url);
		bkp_session_insert(b, "command_key", pupload->command_key);
		bkp_session_insert(b, "file_type", pupload->file_type);
		bkp_session_insert(b, "username", pupload->username);
		bkp_session_insert(b, "password", pupload->password);
		bkp_session_insert(b, "time", schedule_time);
	}
}
void bkp_session_delete_download(struct download *pdownload)
{
	char schedule_time[128];
	char file_size[128];
	mxml_node_t *b;

	snprintf(schedule_time, sizeof(schedule_time), "%lld", (long long int)pdownload->scheduled_time);
	snprintf(file_size, sizeof(file_size), "%d", pdownload->file_size);
	struct search_keywords download_del_keys[7] = { { "url", pdownload->url }, { "command_key", pdownload->command_key }, { "file_type", pdownload->file_type }, { "username", pdownload->username }, { "password", pdownload->password }, { "file_size", file_size }, { "time", schedule_time } };

	b = bkp_session_node_found(bkp_tree, "download", download_del_keys, 7);
	if (b)
		mxmlDelete(b);
}

void bkp_session_delete_schedule_download(struct download *pschedule_download_delete)
{
	char delay[4][128];
	char file_size[128];
	char maxretrie[2][128];
	int i;
	mxml_node_t *b;

	snprintf(file_size, sizeof(file_size), "%d", pschedule_download_delete->file_size);
	for (i = 0; i < 2; i++) {
		snprintf(delay[2 * i], sizeof(delay[i]), "%lld", (long long int)pschedule_download_delete->timewindowstruct[i].windowstart);
		snprintf(delay[2 * i + 1], sizeof(delay[i]), "%lld", (long long int)pschedule_download_delete->timewindowstruct[i].windowend);
		snprintf(maxretrie[i], sizeof(maxretrie[i]), "%d", pschedule_download_delete->timewindowstruct[i].maxretries);
	}
	struct search_keywords sched_download_del_keys[16] = { { "url", pschedule_download_delete->url },
							       { "command_key", pschedule_download_delete->command_key },
							       { "file_type", pschedule_download_delete->file_type },
							       { "username", pschedule_download_delete->username },
							       { "password", pschedule_download_delete->password },
							       { "file_size", file_size },
							       { "windowstart1", delay[0] },
							       { "windowend1", delay[1] },
							       { "windowmode1", pschedule_download_delete->timewindowstruct[0].windowmode },
							       { "usermessage1", pschedule_download_delete->timewindowstruct[0].usermessage },
							       { "maxretrie1", maxretrie[0] },
							       { "windowstart2", delay[2] },
							       { "windowend2", delay[3] },
							       { "windowmode2", pschedule_download_delete->timewindowstruct[1].windowmode },
							       { "usermessage2", pschedule_download_delete->timewindowstruct[1].usermessage },
							       { "maxretrie2", maxretrie[1] } };

	b = bkp_session_node_found(bkp_tree, "schedule_download", sched_download_del_keys, 16);

	if (b)
		mxmlDelete(b);
}
void bkp_session_delete_upload(struct upload *pupload)
{
	char schedule_time[128];
	mxml_node_t *b;

	snprintf(schedule_time, sizeof(schedule_time), "%lld", (long long int)pupload->scheduled_time);
	struct search_keywords upload_del_keys[6] = { { "url", pupload->url }, { "command_key", pupload->command_key }, { "file_type", pupload->file_type }, { "username", pupload->username }, { "password", pupload->password }, { "time", schedule_time } };
	b = bkp_session_node_found(bkp_tree, "upload", upload_del_keys, 6);
	if (b)
		mxmlDelete(b);
}

void bkp_session_insert_autonomous_du_state_change(auto_du_state_change_compl *data)
{
	char resolved[8], fault_code[8];
	mxml_node_t *b;

	if (data == NULL)
		return;

	snprintf(resolved, sizeof(resolved), "%d", data->resolved);
	snprintf(fault_code, sizeof(fault_code), "%d", data->fault_code);

	struct search_keywords keys[9] = {
					   { "uuid", data->uuid },
					   { "version", data->ver ? data->ver : "" },
					   { "current_state", data->current_state ? data->current_state : "" },
					   { "resolved", resolved },
					   { "start_time", data->start_time ? data->start_time : "" },
					   { "complete_time", data->complete_time ? data->complete_time : "" },
					   { "fault_code", fault_code },
					   { "fault_string", data->fault_string ? data->fault_string : "" },
					   { "operation", data->operation ? data->operation : "" }
					 };

	b = bkp_session_node_found(bkp_tree, "autonomous_du_state_change_complete", keys, 9);
	if (!b) {
		b = bkp_session_insert(bkp_tree, "autonomous_du_state_change_complete", NULL);
		bkp_session_insert(b, "uuid", data->uuid);
		bkp_session_insert(b, "version", data->ver ? data->ver : "");
		bkp_session_insert(b, "current_state", data->current_state ? data->current_state : "");
		bkp_session_insert(b, "resolved", resolved);
		bkp_session_insert(b, "start_time", data->start_time ? data->start_time : "");
		bkp_session_insert(b, "complete_time", data->complete_time ? data->complete_time : "");
		bkp_session_insert(b, "fault_code", fault_code);
		bkp_session_insert(b, "fault_string", data->fault_string ? data->fault_string : "");
		bkp_session_insert(b, "operation", data->operation ? data->operation : "");
	}
}

void bkp_session_delete_autonomous_du_state_change(auto_du_state_change_compl *data)
{
	char resolved[8], fault_code[8];
	mxml_node_t *b;

	if (data == NULL)
		return;

	snprintf(resolved, sizeof(resolved), "%d", data->resolved);
	snprintf(fault_code, sizeof(fault_code), "%d", data->fault_code);

	struct search_keywords keys[9] = {
	   { "uuid", data->uuid },
	   { "version", data->ver ? data->ver : "" },
	   { "current_state", data->current_state ? data->current_state : "" },
	   { "resolved", resolved },
	   { "start_time", data->start_time ? data->start_time : "" },
	   { "complete_time", data->complete_time ? data->complete_time : "" },
	   { "fault_code", fault_code },
	   { "fault_string", data->fault_string ? data->fault_string : "" },
	   { "operation", data->operation ? data->operation : "" }
	};

	b = bkp_session_node_found(bkp_tree, "autonomous_du_state_change_complete", keys, 9);
	if (!b) {
		mxmlDelete(b);
	}
}

void bkp_session_insert_autonomous_transfer_complete(auto_transfer_complete *data)
{
	char is_download[8], fault_code[8], file_size[8];
	mxml_node_t *b;

	if (data == NULL)
		return;

	snprintf(is_download, sizeof(is_download), "%d", data->is_download);
	snprintf(file_size, sizeof(is_download), "%d", data->file_size);
	snprintf(fault_code, sizeof(fault_code), "%d", data->fault_code);

	struct search_keywords keys[9] = {
					   { "announceurl", data->announce_url ? data->announce_url : "" },
					   { "transferurl", data->transfer_url ? data->transfer_url : "" },
					   { "isdownload", is_download },
					   { "filetype", data->file_type },
					   { "filesize", file_size },
					   { "start_time", data->start_time ? data->start_time : "" },
					   { "complete_time", data->complete_time ? data->complete_time : "" },
					   { "fault_code", fault_code },
					   { "fault_string", data->fault_string ? data->fault_string : "" }
	};

	b = bkp_session_node_found(bkp_tree, "autonomous_transfer_complete", keys, 9);
	if (!b) {
		b = bkp_session_insert(bkp_tree, "autonomous_transfer_complete", NULL);
		bkp_session_insert(b, "announceurl", data->announce_url ? data->announce_url : "");
		bkp_session_insert(b, "transferurl", data->transfer_url ? data->transfer_url : "");
		bkp_session_insert(b, "isdownload", is_download);
		bkp_session_insert(b, "filetype", data->file_type);
		bkp_session_insert(b, "filesize", file_size);
		bkp_session_insert(b, "start_time", data->start_time ? data->start_time : "");
		bkp_session_insert(b, "complete_time", data->complete_time ? data->complete_time : "");
		bkp_session_insert(b, "fault_code", fault_code);
		bkp_session_insert(b, "fault_string", data->fault_string ? data->fault_string : "");
	}
}

void bkp_session_delete_autonomous_transfer_complete(auto_transfer_complete *data)
{
	if (data == NULL)
		return;

	char is_download[8], fault_code[8], file_size[8];
	snprintf(is_download, sizeof(is_download), "%d", data->is_download);
	snprintf(file_size, sizeof(file_size), "%d", data->file_size);
	snprintf(fault_code, sizeof(fault_code), "%d", data->fault_code);

	struct search_keywords keys[9] = {
	   { "announceurl", data->announce_url ? data->announce_url : "" },
	   { "transferurl", data->transfer_url ? data->transfer_url : "" },
	   { "isdownload", is_download },
	   { "filetype", data->file_type },
	   { "filesize", file_size },
	   { "start_time", data->start_time ? data->start_time : "" },
	   { "complete_time", data->complete_time ? data->complete_time : "" },
	   { "fault_code", fault_code },
	   { "fault_string", data->fault_string ? data->fault_string : "" }
	};

	mxml_node_t *b = bkp_session_node_found(bkp_tree, "autonomous_transfer_complete", keys, 9);
	if (!b) {
		mxmlDelete(b);
	}
}

void bkp_session_insert_du_state_change_complete(struct du_state_change_complete *pdu_state_change_complete)
{
	char schedule_time[128], resolved[8], fault_code[8];
	struct opresult *p = NULL;
	mxml_node_t *b;

	snprintf(schedule_time, sizeof(schedule_time), "%lld", (long long int)pdu_state_change_complete->timeout);
	b = bkp_session_insert(bkp_tree, "du_state_change_complete", NULL);
	bkp_session_insert(b, "command_key", pdu_state_change_complete->command_key);
	bkp_session_insert(b, "time", schedule_time);
	list_for_each_entry (p, &(pdu_state_change_complete->list_opresult), list) {
		mxml_node_t *n;
		n = bkp_session_insert(b, "opresult", NULL);
		snprintf(resolved, sizeof(resolved), "%d", p->resolved);
		snprintf(fault_code, sizeof(fault_code), "%d", p->fault);
		bkp_session_insert(n, "uuid", p->uuid);
		bkp_session_insert(n, "du_ref", p->du_ref);
		bkp_session_insert(n, "version", p->version);
		bkp_session_insert(n, "current_state", p->current_state);
		bkp_session_insert(n, "resolved", resolved);
		bkp_session_insert(n, "execution_unit_ref", p->execution_unit_ref);
		bkp_session_insert(n, "start_time", p->start_time);
		bkp_session_insert(n, "complete_time", p->complete_time);
		bkp_session_insert(n, "fault", fault_code);
	}
}

void bkp_session_delete_du_state_change_complete(struct du_state_change_complete *pdu_state_change_complete)
{
	mxml_node_t *b;
	char schedule_time[128];

	snprintf(schedule_time, sizeof(schedule_time), "%lld", (long long int)pdu_state_change_complete->timeout);
	struct search_keywords cds_complete_keys[2] = { { "command_key", pdu_state_change_complete->command_key }, { "time", schedule_time } };

	b = bkp_session_node_found(bkp_tree, "du_state_change_complete", cds_complete_keys, 2);
	if (b)
		mxmlDelete(b);
}
void bkp_session_insert_transfer_complete(struct transfer_complete *ptransfer_complete)
{
	struct search_keywords keys[5];
	char fault_code[16];
	char type[16];
	mxml_node_t *b;

	snprintf(fault_code, sizeof(fault_code), "%d", ptransfer_complete->fault_code);
	keys[0].name = "command_key";
	keys[0].value = ptransfer_complete->command_key;
	keys[1].name = "start_time";
	keys[1].value = ptransfer_complete->start_time;
	keys[2].name = "complete_time";
	keys[2].value = ptransfer_complete->complete_time;
	keys[3].name = "fault_code";
	keys[3].value = fault_code;
	keys[4].name = "type";
	snprintf(type, sizeof(type), "%d", ptransfer_complete->type);
	keys[4].value = type;
	b = bkp_session_node_found(bkp_tree, "transfer_complete", keys, 5);
	if (!b) {
		b = bkp_session_insert(bkp_tree, "transfer_complete", NULL);
		bkp_session_insert(b, "command_key", ptransfer_complete->command_key);
		bkp_session_insert(b, "start_time", ptransfer_complete->start_time);
		bkp_session_insert(b, "complete_time", ptransfer_complete->complete_time);
		bkp_session_insert(b, "old_software_version", ptransfer_complete->old_software_version);
		bkp_session_insert(b, "fault_code", fault_code);
		bkp_session_insert(b, "type", type);
	}
}

void bkp_session_delete_transfer_complete(struct transfer_complete *ptransfer_complete)
{
	char fault_code[16];
	char type[16];
	mxml_node_t *b;

	snprintf(fault_code, sizeof(fault_code), "%d", ptransfer_complete->fault_code);
	snprintf(type, sizeof(type), "%d", ptransfer_complete->type);
	struct search_keywords trans_comp_del_keys[5] = { { "command_key", ptransfer_complete->command_key }, { "start_time", ptransfer_complete->start_time }, { "complete_time", ptransfer_complete->complete_time }, { "fault_code", fault_code }, { "type", type } };

	b = bkp_session_node_found(bkp_tree, "transfer_complete", trans_comp_del_keys, 5);
	if (b)
		mxmlDelete(b);
}

int save_acs_bkp_config()
{
	struct config *conf;

	conf = &(cwmp_main->conf);
	bkp_session_simple_insert("acs", "url", conf->acsurl);
	bkp_session_save();
	return CWMP_OK;
}

/*
 * Load backup session
 */
char *load_child_value(mxml_node_t *tree, char *sub_name)
{
	char *value = NULL;
	mxml_node_t *b = tree;

	if (b) {
		b = mxmlFindElement(b, b, sub_name, NULL, NULL, MXML_DESCEND);
		if (b) {
			b = mxmlWalkNext(b, tree, MXML_DESCEND);
			if (b && mxmlGetType(b) == MXML_OPAQUE) {
				const char *opaque = mxmlGetOpaque(b);
				if (opaque != NULL) {
					value = strdup(opaque);
				}
			}
		}
	}
	return value;
}

void load_queue_event(mxml_node_t *tree)
{
	char *command_key = NULL;
	mxml_node_t *b = tree, *c;
	int idx = -1, id = -1;
	struct event_container *event_container_save = NULL;

	struct backup_attributes bkp_attrs = { .index = &idx, .id = &id, .command_key = &command_key };
	load_specific_backup_attributes(tree, &bkp_attrs);

	b = mxmlWalkNext(b, tree, MXML_DESCEND);

	while (b) {
		if (mxmlGetType(b) == MXML_ELEMENT) {
			const char *element = mxmlGetElement(b);
			if (element == NULL) {
				b = mxmlWalkNext(b, tree, MXML_NO_DESCEND);
				continue;
			}
			if (strcmp(element, "command_key") == 0) {
				// cppcheck-suppress knownConditionTrueFalse
				/*
				 * idx value can be modified when loading the backup with the function load_specific_backup_attributes
				 */
				if (idx != -1) {
					if (EVENT_CONST[idx].RETRY & EVENT_RETRY_AFTER_REBOOT) {
						event_container_save = cwmp_add_event_container(idx, ((command_key != NULL) ? command_key : ""));
						if (event_container_save != NULL) {
							event_container_save->id = id;
						}
					}
				}
				FREE(command_key);
			} else if (strcmp(element, "parameter") == 0) {
				c = mxmlWalkNext(b, b, MXML_DESCEND);
				if (c && mxmlGetType(c) == MXML_OPAQUE) {
					const char *op = mxmlGetOpaque(c);
					if (op != NULL) {
						if (event_container_save != NULL) {
							add_dm_parameter_to_list(&(event_container_save->head_dm_parameter), (char *)op, NULL, NULL, 0, false);
						}
					}
				}
			}
		}
		b = mxmlWalkNext(b, tree, MXML_NO_DESCEND);
	}
}

void load_schedule_inform(mxml_node_t *tree)
{
	char *command_key = NULL;
	time_t scheduled_time = 0;
	struct schedule_inform *schedule_inform = NULL;
	struct list_head *ilist = NULL;

	struct backup_attributes bkp_attrs = { .command_key = &command_key, .time = &scheduled_time };
	load_specific_backup_attributes(tree, &bkp_attrs);

	list_for_each (ilist, &(list_schedule_inform)) {
		schedule_inform = list_entry(ilist, struct schedule_inform, list);
		if (schedule_inform->scheduled_time > scheduled_time) {
			break;
		}
	}
	schedule_inform = calloc(1, sizeof(struct schedule_inform));
	if (schedule_inform != NULL) {
		schedule_inform->commandKey = command_key;
		schedule_inform->scheduled_time = scheduled_time;
		list_add(&(schedule_inform->list), ilist->prev);
	}
}

void load_download(mxml_node_t *tree)
{
	struct download *download_request = NULL;
	struct list_head *ilist = NULL;
	struct download *idownload_request = NULL;

	if (tree == NULL) {
		CWMP_LOG(ERROR, "backup %s: tree is null", __FUNCTION__);
		return;
	}
	download_request = calloc(1, sizeof(struct download));
	if (download_request == NULL) {
		CWMP_LOG(ERROR, "backup %s: download_request is null", __FUNCTION__);
		return;
	}
	struct backup_attributes bkp_attrs = { .url = &download_request->url,
					       .command_key = &download_request->command_key,
					       .file_type = &download_request->file_type,
					       .username = &download_request->username,
					       .password = &download_request->password,
					       .file_size = &download_request->file_size,
					       .time = &download_request->scheduled_time };
	load_specific_backup_attributes(tree, &bkp_attrs);
	download_request->handler_timer.cb = cwmp_start_download;

	list_for_each (ilist, &(list_download)) {
		idownload_request = list_entry(ilist, struct download, list);
		if (idownload_request->scheduled_time > download_request->scheduled_time) {
			break;
		}
	}
	list_add(&(download_request->list), ilist->prev);
	if (download_request->scheduled_time != 0)
		count_download_queue++;
	cwmp_set_end_session(END_SESSION_DOWNLOAD);
}

void load_schedule_download(mxml_node_t *tree)
{
	struct download *download_request = NULL;
	struct list_head *ilist = NULL;
	struct download *idownload_request = NULL;

	if (tree == NULL) {
		CWMP_LOG(ERROR, "backup %s: tree is null", __FUNCTION__);
		return;
	}
	download_request = calloc(1, sizeof(struct download));
	if (download_request == NULL) {
		CWMP_LOG(ERROR, "backup %s: download_request is null", __FUNCTION__);
		return;
	}

	struct backup_attributes bkp_attrs = {
		.url = &download_request->url,
		.command_key = &download_request->command_key,
		.file_type = &download_request->file_type,
		.username = &download_request->username,
		.password = &download_request->password,
		.file_size = &download_request->file_size,
		.windowstart1 = &download_request->timewindowstruct[0].windowstart,
		.windowend1 = &download_request->timewindowstruct[0].windowend,
		.windowmode1 = &download_request->timewindowstruct[0].windowmode,
		.usermessage1 = &download_request->timewindowstruct[0].usermessage,
		.maxretrie1 = &download_request->timewindowstruct[0].maxretries,
		.windowstart2 = &download_request->timewindowstruct[1].windowstart,
		.windowend2 = &download_request->timewindowstruct[1].windowend,
		.windowmode2 = &download_request->timewindowstruct[1].windowmode,
		.usermessage2 = &download_request->timewindowstruct[1].usermessage,
		.maxretrie2 = &download_request->timewindowstruct[1].maxretries,
	};
	load_specific_backup_attributes(tree, &bkp_attrs);

	list_for_each (ilist, &(list_schedule_download)) {
		idownload_request = list_entry(ilist, struct download, list);
		if (idownload_request->timewindowstruct[0].windowstart > download_request->timewindowstruct[0].windowstart) {
			break;
		}
	}
	list_add(&(download_request->list), ilist->prev);
	if (download_request->timewindowstruct[0].windowstart != 0)
		count_download_queue++;
}

void load_upload(mxml_node_t *tree)
{
	struct upload *upload_request = NULL;
	struct list_head *ilist = NULL;
	struct upload *iupload_request = NULL;

	if (tree == NULL) {
		CWMP_LOG(ERROR, "backup %s: tree is null", __FUNCTION__);
		return;
	}
	upload_request = calloc(1, sizeof(struct upload));
	if (upload_request == NULL) {
		CWMP_LOG(ERROR, "backup %s: download_request is null", __FUNCTION__);
		return;
	}

	struct backup_attributes bkp_attrs = { .url = &upload_request->url, .command_key = &upload_request->command_key, .file_type = &upload_request->file_type, .username = &upload_request->username, .password = &upload_request->password, .time = &upload_request->scheduled_time };
	load_specific_backup_attributes(tree, &bkp_attrs);

	list_for_each (ilist, &(list_upload)) {
		iupload_request = list_entry(ilist, struct upload, list);
		if (iupload_request->scheduled_time > upload_request->scheduled_time) {
			break;
		}
	}
	list_add(&(upload_request->list), ilist->prev);
	if (upload_request->scheduled_time != 0)
		count_upload_queue++;
}

void load_change_du_state(mxml_node_t *tree)
{
	if (tree == NULL) {
		CWMP_LOG(ERROR, "backup %s: tree is null", __FUNCTION__);
		return;
	}
	mxml_node_t *b = tree;
	struct change_du_state *change_du_state_request = NULL;
	struct operations *elem;

	change_du_state_request = calloc(1, sizeof(struct change_du_state));
	INIT_LIST_HEAD(&(change_du_state_request->list_operation));

	struct backup_attributes bkp_attrs = { .command_key = &change_du_state_request->command_key, .time = &change_du_state_request->timeout };
	load_specific_backup_attributes(tree, &bkp_attrs);

	b = mxmlWalkNext(b, tree, MXML_DESCEND);

	while (b) {
		if (mxmlGetType(b) == MXML_ELEMENT) {
			const char *element = mxmlGetElement(b);
			if (strcmp(element, "update") == 0) {
				elem = (operations *)calloc(1, sizeof(operations));
				elem->type = DU_UPDATE;
				list_add_tail(&(elem->list), &(change_du_state_request->list_operation));
				struct backup_attributes update_bkp_attrs = { .uuid = &elem->uuid, .version = &elem->version, .url = &elem->url, .username = &elem->username, .password = &elem->password };
				load_specific_backup_attributes(b, &update_bkp_attrs);
			} else if (strcmp(element, "install") == 0) {
				elem = (operations *)calloc(1, sizeof(operations));
				elem->type = DU_INSTALL;
				list_add_tail(&(elem->list), &(change_du_state_request->list_operation));

				struct backup_attributes install_bkp_attrs = { .uuid = &elem->uuid, .executionenvref = &elem->executionenvref, .url = &elem->url, .username = &elem->username, .password = &elem->password };
				load_specific_backup_attributes(b, &install_bkp_attrs);
			} else if (strcmp(element, "uninstall") == 0) {
				elem = (operations *)calloc(1, sizeof(operations));
				elem->type = DU_UNINSTALL;
				list_add_tail(&(elem->list), &(change_du_state_request->list_operation));
				struct backup_attributes uninstall_bkp_attrs = { .uuid = &elem->uuid, .version = &elem->version, .executionenvref = &elem->executionenvref };
				load_specific_backup_attributes(b, &uninstall_bkp_attrs);
			}
		}
		b = mxmlWalkNext(b, tree, MXML_NO_DESCEND);
	}
	list_add_tail(&(change_du_state_request->list_operation), &(list_change_du_state));
}

void load_du_state_change_complete(mxml_node_t *tree)
{
	mxml_node_t *b = tree;
	struct du_state_change_complete *du_state_change_complete_request = NULL;
	struct opresult *elem;

	du_state_change_complete_request = calloc(1, sizeof(struct du_state_change_complete));
	INIT_LIST_HEAD(&(du_state_change_complete_request->list_opresult));

	struct backup_attributes bkp_attrs = { .command_key = &du_state_change_complete_request->command_key, .time = &du_state_change_complete_request->timeout };
	load_specific_backup_attributes(tree, &bkp_attrs);

	b = mxmlWalkNext(b, tree, MXML_DESCEND);

	while (b) {
		if (mxmlGetType(b) == MXML_ELEMENT) {
			if (strcmp(mxmlGetElement(b), "opresult") == 0) {
				elem = (opresult *)calloc(1, sizeof(opresult));
				list_add_tail(&(elem->list), &(du_state_change_complete_request->list_opresult));

				struct backup_attributes opresult_bkp_attrs = { .uuid = &elem->uuid,
										.version = &elem->version,
										.du_ref = &elem->du_ref,
										.current_state = &elem->current_state,
										.resolved = &elem->resolved,
										.start_time = &elem->start_time,
										.complete_time = &elem->complete_time,
										.fault = &elem->fault,
										.execution_unit_ref = &elem->execution_unit_ref };
				load_specific_backup_attributes(b, &opresult_bkp_attrs);
			}
		}
		b = mxmlWalkNext(b, tree, MXML_NO_DESCEND);
	}
	cwmp_root_cause_changedustate_complete(du_state_change_complete_request);
}

void load_transfer_complete(mxml_node_t *tree)
{
	struct transfer_complete *ptransfer_complete;

	ptransfer_complete = calloc(1, sizeof(struct transfer_complete));

	struct backup_attributes bkp_attrs = { .command_key = &ptransfer_complete->command_key,
					       .start_time = &ptransfer_complete->start_time,
					       .complete_time = &ptransfer_complete->complete_time,
					       .old_software_version = &ptransfer_complete->old_software_version,
					       .fault_code = &ptransfer_complete->fault_code,
					       .type = &ptransfer_complete->type };
	load_specific_backup_attributes(tree, &bkp_attrs);

	cwmp_root_cause_transfer_complete(ptransfer_complete);
	sotfware_version_value_change(ptransfer_complete);
}

void load_autonomous_du_state_change_complete(mxml_node_t *tree)
{
	auto_du_state_change_compl *p;

	p = calloc(1, sizeof(auto_du_state_change_compl));

	struct backup_attributes bkp_attrs = { .uuid = &p->uuid,
					       .version = &p->ver,
					       .current_state = &p->current_state,
					       .resolved = &p->resolved, 
					       .start_time = &p->start_time,
					       .complete_time = &p->complete_time,
					       .fault_code = &p->fault_code,
					       .fault_string = &p->fault_string,
					       .operation = &p->operation };
	load_specific_backup_attributes(tree, &bkp_attrs);

	cwmp_root_cause_autonomous_cdu_complete(p);
}

void bkp_session_create_file()
{
	FILE *pFile;

	pFile = fopen(CWMP_BKP_FILE, "w");
	if (pFile == NULL) {
		CWMP_LOG(ERROR, "Unable to create %s file", CWMP_BKP_FILE);
		return;
	}
	fprintf(pFile, "%s", CWMP_BACKUP_SESSION);
	if (bkp_tree != NULL)
		MXML_DELETE(bkp_tree);
	bkp_tree = mxmlLoadString(NULL, CWMP_BACKUP_SESSION, MXML_OPAQUE_CALLBACK);
	fclose(pFile);
}

int bkp_session_check_file()
{
	if (!file_exists(CWMP_BKP_FILE)) {
		bkp_session_create_file();
		return -1;
	}

	if (bkp_tree == NULL) {
		FILE *pFile;
		pFile = fopen(CWMP_BKP_FILE, "r");
		bkp_tree = mxmlLoadFile(NULL, pFile, MXML_OPAQUE_CALLBACK);
		fclose(pFile);
	}

	if (bkp_tree == NULL) {
		bkp_session_create_file();
		return -1;
	}
	bkp_session_save();
	return 0;
}

int cwmp_init_backup_session(char **ret, enum backup_loading load)
{
	int error;
	if (bkp_session_check_file())
		return 0;
	error = cwmp_load_saved_session(ret, load);
	return error;
}

int cwmp_load_saved_session(char **ret, enum backup_loading load)
{
	mxml_node_t *b;

	b = bkp_tree;
	b = mxmlWalkNext(b, bkp_tree, MXML_DESCEND);
	while (b) {
		mxml_type_t ntype = mxmlGetType(b);
		const char *elem_name = mxmlGetElement(b);
		if (load == ACS) {
			if (ntype == MXML_ELEMENT && strcmp(elem_name, "acs") == 0) {
				*ret = load_child_value(b, "url");
				break;
			}
		}
		if (load == CR_IP) {
			if (ntype == MXML_ELEMENT && strcmp(elem_name, "connection_request") == 0) {
				*ret = load_child_value(b, "ip");
				break;
			}
		}
		if (load == CR_IPv6) {
			if (ntype == MXML_ELEMENT && strcmp(elem_name, "connection_request") == 0) {
				*ret = load_child_value(b, "ipv6");
				break;
			}
		}
		if (load == CR_PORT) {
			if (ntype == MXML_ELEMENT && strcmp(elem_name, "connection_request") == 0) {
				*ret = load_child_value(b, "port");
				break;
			}
		}
		if (load == ALL) {
			if (ntype == MXML_ELEMENT && strcmp(elem_name, "cwmp_event") == 0) {
				load_queue_event(b);
			} else if (ntype == MXML_ELEMENT && strcmp(elem_name, "download") == 0) {
				load_download(b);
			} else if (ntype == MXML_ELEMENT && strcmp(elem_name, "upload") == 0) {
				load_upload(b);
			} else if (ntype == MXML_ELEMENT && strcmp(elem_name, "transfer_complete") == 0) {
				load_transfer_complete(b);
			} else if (ntype == MXML_ELEMENT && strcmp(elem_name, "schedule_inform") == 0) {
				load_schedule_inform(b);
			} else if (ntype == MXML_ELEMENT && strcmp(elem_name, "change_du_state") == 0) {
				load_change_du_state(b);
			} else if (ntype == MXML_ELEMENT && strcmp(elem_name, "du_state_change_complete") == 0) {
				load_du_state_change_complete(b);
			} else if (ntype == MXML_ELEMENT && strcmp(elem_name, "schedule_download") == 0) {
				load_schedule_download(b);
			} else if (ntype == MXML_ELEMENT && strcmp(elem_name, "autonomous_du_state_change_complete") == 0) {
				load_autonomous_du_state_change_complete(b);
			}
		}
		b = mxmlWalkNext(b, bkp_tree, MXML_NO_DESCEND);
	}

	return CWMP_OK;
}
