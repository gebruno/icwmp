/*
 * upload.c - Upload method corresponding functions
 *
 * Copyright (C) 2021-2022, IOPSYS Software Solutions AB.
 *
 *	  Author Omar Kallel <omar.kallel@pivasoftware.com>
 *
 * See LICENSE file for license related information.
 *
 */

#include <curl/curl.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "upload.h"
#include "datamodel_interface.h"
#include "log.h"
#include "backupSession.h"
#include "event.h"
#include "cwmp_uci.h"
#include "subprocess.h"
#include "session.h"

#define CURL_TIMEOUT 20

int count_upload_queue = 0;

LIST_HEAD(list_upload);

int lookup_vcf_name(int instance, char **value)
{
	char vcf_name_parameter[256];
	LIST_HEAD(vcf_parameters);
	snprintf(vcf_name_parameter, sizeof(vcf_name_parameter), "Device.DeviceInfo.VendorConfigFile.%d.Name", instance);
	if (cwmp_get_parameter_values(vcf_name_parameter, &vcf_parameters) != NULL) {
		CWMP_LOG(ERROR, "Not able to get the value of the parameter %s", vcf_name_parameter);
		return -1;
	}
	struct cwmp_dm_parameter *param_value = NULL;
	list_for_each_entry (param_value, &vcf_parameters, list) {
		*value = param_value->value ? strdup(param_value->value) : NULL;
		break;
	}
	cwmp_free_all_dm_parameter_list(&vcf_parameters);
	return 0;
}

int lookup_vlf_name(int instance, char **value)
{
	char vlf_name_parameter[256];
	LIST_HEAD(vlf_parameters);
	snprintf(vlf_name_parameter, sizeof(vlf_name_parameter), "Device.DeviceInfo.VendorLogFile.%d.Name", instance);
	if (cwmp_get_parameter_values(vlf_name_parameter, &vlf_parameters) != NULL) {
		CWMP_LOG(ERROR, "Not able to get the value of the parameter %s", vlf_name_parameter);
		return -1;
	}
	struct cwmp_dm_parameter *param_value = NULL;
	list_for_each_entry (param_value, &vlf_parameters, list) {
		*value = param_value->value ? strdup(param_value->value) : NULL;
		break;
	}
	cwmp_free_all_dm_parameter_list(&vlf_parameters);
	return 0;
}

/*
 * Upload file
 */
int upload_file(const char *file_path, const char *url, const char *username, const char *password)
{
	int res_code = 0;
	CURL *curl;
	CURLcode res;
	FILE *fd_upload;
	struct stat file_info;

	if (url == NULL) {
		CWMP_LOG(ERROR, "upload %s: url is null", __FUNCTION__);
		return -1;
	}
	if (file_path == NULL) {
		file_path = "/tmp/upload_file";
	}
	stat(file_path, &file_info);
	fd_upload = fopen(file_path, "rb");
	if (fd_upload == NULL) {
		CWMP_LOG(ERROR, "Failed to open url[%s] for upload", file_path);
		return FAULT_CPE_INTERNAL_ERROR;
	}
	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();

	if (curl) {
		if (username != NULL && strlen(username) > 0) {
			char userpass[256];
			snprintf(userpass, sizeof(userpass), "%s:%s", username, password ? password : "");
			curl_easy_setopt(curl, CURLOPT_USERPWD, userpass);
		}
		if (strncmp(url, "https://", 8) == 0)
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, CURL_TIMEOUT);
		curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 50L);
		curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
		//curl_easy_setopt(curl, CURLOPT_POST, 1L);
		curl_easy_setopt(curl, CURLOPT_HTTPAUTH, (long)CURLAUTH_ANY);
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_READDATA, fd_upload);
		curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)file_info.st_size);
		res = curl_easy_perform(curl);
		if(res != CURLE_OK) {
			CWMP_LOG(ERROR, "## curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		}

		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &res_code);
		curl_easy_cleanup(curl);
	}
	fclose(fd_upload);
	curl_global_cleanup();

	return res_code;
}

char *upload_file_task_function(char *task)
{

	struct blob_buf bbuf;

	if (task == NULL) {
		CWMP_LOG(ERROR, "upload %s: task is null", __FUNCTION__);
		return NULL;
	}
	memset(&bbuf, 0, sizeof(struct blob_buf));
	blob_buf_init(&bbuf, 0);

	if (blobmsg_add_json_from_string(&bbuf, task) == false) {
		blob_buf_free(&bbuf);
		return NULL;
	}
	const struct blobmsg_policy p[5] = { { "task", BLOBMSG_TYPE_STRING }, { "file_path", BLOBMSG_TYPE_STRING }, { "url", BLOBMSG_TYPE_STRING }, { "username", BLOBMSG_TYPE_STRING }, { "password", BLOBMSG_TYPE_STRING } };

	struct blob_attr *tb[5] = { NULL, NULL, NULL, NULL, NULL};
	blobmsg_parse(p, 5, tb, blobmsg_data(bbuf.head), blobmsg_len(bbuf.head));
	char *task_name = blobmsg_get_string(tb[0]);
	if (!task_name || strcmp(task_name, "upload") != 0)
		return NULL;
	char *file_path = blobmsg_get_string(tb[1]);
	char *url = blobmsg_get_string(tb[2]);
	char *username = blobmsg_get_string(tb[3]);
	char *password = blobmsg_get_string(tb[4]);

	int http_code = upload_file(file_path, url, username, password);
	char *http_ret = (char *)malloc(4 * sizeof(char));
	snprintf(http_ret, 4, "%d", http_code);
	http_ret[3] = 0;
	return http_ret;
}

int upload_file_in_subprocess(const char *file_path, const char *url, const char *username, const char *password)
{
	if (url == NULL) {
		CWMP_LOG(ERROR, "upload %s: url is null");
		return 500;
	}
	subprocess_start(upload_file_task_function);

	struct blob_buf bbuf;
	memset(&bbuf, 0, sizeof(struct blob_buf));
	blob_buf_init(&bbuf, 0);
	blobmsg_add_string(&bbuf, "task", "upload");
	blobmsg_add_string(&bbuf, "file_path", file_path);
	blobmsg_add_string(&bbuf, "url", url);
	blobmsg_add_string(&bbuf, "username", username);
	blobmsg_add_string(&bbuf, "password", password);
	char *upload_task = blobmsg_format_json(bbuf.head, true);
	blob_buf_free(&bbuf);

	if (upload_task != NULL) {
		char *ret = execute_task_in_subprocess(upload_task);
		return atoi(ret);
	}
	return 500;
}

int cwmp_launch_upload(struct upload *pupload, struct transfer_complete **ptransfer_complete)
{
	int error = FAULT_CPE_NO_FAULT;
	char *upload_startTime;
	struct transfer_complete *p;
	char *name = NULL;
	upload_startTime = get_time(time(NULL));
	char file_path[128] = {'\0'};
	bkp_session_delete_upload(pupload);
	bkp_session_save();

	if (pupload->file_type[0] == '1') {
		snprintf(file_path, sizeof(file_path), "/tmp/all_configs");
		cwmp_uci_init();
		cwmp_uci_export(file_path, UCI_STANDARD_CONFIG);
		cwmp_uci_exit();
	} else if (pupload->file_type[0] == '2') {
		snprintf(file_path, sizeof(file_path), "/tmp/syslog");
		copy("/var/log/syslog", file_path);
	} else if (pupload->file_type[0] == '3') {
		lookup_vcf_name(pupload->f_instance, &name);
		if (name && strlen(name) > 0) {
			// cppcheck-suppress uninitvar
			snprintf(file_path, sizeof(file_path), "/tmp/%s", name);
			cwmp_uci_init();
			cwmp_uci_export_package(name, file_path, UCI_STANDARD_CONFIG);
			cwmp_uci_exit();
			FREE(name);
		} else {
			error = FAULT_CPE_UPLOAD_FAILURE;
			goto end_upload;
		}
	} else { //file_type is 4
		lookup_vlf_name(pupload->f_instance, &name);
		if (name && strlen(name) > 0) {
			snprintf(file_path, sizeof(file_path), "/tmp/%s", name);
			copy(name, file_path);
			FREE(name);
		} else
			error = FAULT_CPE_UPLOAD_FAILURE;
	}

	if (error != FAULT_CPE_NO_FAULT || strlen(file_path) == 0) {
		error = FAULT_CPE_UPLOAD_FAILURE;
		goto end_upload;
	}

	int ret = upload_file_in_subprocess(file_path, pupload->url, pupload->username, pupload->password);
	if (ret == 200 || ret == 204)
		error = FAULT_CPE_NO_FAULT;
	else
		error = FAULT_CPE_UPLOAD_FAILURE;
	remove(file_path);

end_upload:
	p = calloc(1, sizeof(struct transfer_complete));
	if (p == NULL || ptransfer_complete == NULL) {
		error = FAULT_CPE_INTERNAL_ERROR;
		return error;
	}

	p->command_key = pupload->command_key ? strdup(pupload->command_key) : strdup("");
	p->start_time = strdup(upload_startTime);
	p->complete_time = strdup(get_time(time(NULL)));
	p->type = TYPE_UPLOAD;
	if (error != FAULT_CPE_NO_FAULT) {
		p->fault_code = error;
	}

	*ptransfer_complete = p;
	return error;
}

int cwmp_free_upload_request(struct upload *upload)
{
	if (upload != NULL) {
		if (upload->command_key != NULL)
			FREE(upload->command_key);

		if (upload->file_type != NULL)
			FREE(upload->file_type);

		if (upload->url != NULL)
			FREE(upload->url);

		if (upload->username != NULL)
			FREE(upload->username);

		if (upload->password != NULL)
			FREE(upload->password);

		FREE(upload);
	}
	return CWMP_OK;
}

int cwmp_scheduledUpload_remove_all()
{
	while (list_upload.next != &(list_upload)) {
		struct upload *upload;
		upload = list_entry(list_upload.next, struct upload, list);
		list_del(&(upload->list));
		bkp_session_delete_upload(upload);
		if (upload->scheduled_time != 0)
			count_upload_queue--;
		cwmp_free_upload_request(upload);
	}
	return CWMP_OK;
}

void cwmp_start_upload(struct uloop_timeout *timeout)
{
	struct upload *pupload;
	int error = FAULT_CPE_NO_FAULT;
	struct transfer_complete *ptransfer_complete;

	pupload = container_of(timeout, struct upload, handler_timer);

	CWMP_LOG(INFO, "Launch upload file %s", pupload->url);
	error = cwmp_launch_upload(pupload, &ptransfer_complete);
	sleep(3);
	if (error != FAULT_CPE_NO_FAULT) {
		CWMP_LOG(ERROR, "Error while uploading the file: %s", pupload->url);
	}

	bkp_session_insert_transfer_complete(ptransfer_complete);
	bkp_session_save();
	list_del(&(pupload->list));
	if (pupload->scheduled_time != 0)
		count_upload_queue--;
	cwmp_free_upload_request(pupload);

	struct session_timer_event *upload_inform_event = calloc(1, sizeof(struct session_timer_event));

	upload_inform_event->extra_data = ptransfer_complete;
	upload_inform_event->session_timer_evt.cb = cwmp_schedule_session_with_event;
	upload_inform_event->event = TransferClt_Evt;
	trigger_cwmp_session_timer_with_event(&upload_inform_event->session_timer_evt);
}

void apply_upload()
{
	struct list_head *ilist;
	list_for_each (ilist, &(list_upload)) {
		struct upload *upload = list_entry(ilist, struct upload, list);
		int upload_delay = 0;
		if (upload->scheduled_time > time(NULL)) {
			upload_delay = upload->scheduled_time - time(NULL);
		}
		uloop_timeout_set(&upload->handler_timer, 1000 * upload_delay);
	}
}
