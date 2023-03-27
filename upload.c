/*
 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	Copyright (C) 2013-2021 iopsys Software Solutions AB
 *	  Author Omar Kallel <omar.kallel@pivasoftware.com>
 */

#include <curl/curl.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "upload.h"
#include "download.h"
#include "datamodel_interface.h"
#include "log.h"
#include "backupSession.h"
#include "event.h"
#include "cwmp_uci.h"

#define CURL_TIMEOUT 20

LIST_HEAD(list_upload);

pthread_cond_t threshold_upload;
pthread_mutex_t mutex_upload = PTHREAD_MUTEX_INITIALIZER;

int lookup_vcf_name(int instance, char **value)
{
	char vcf_name_parameter[256];
	LIST_HEAD(vcf_parameters);
	snprintf(vcf_name_parameter, sizeof(vcf_name_parameter), "Device.DeviceInfo.VendorConfigFile.%d.Name", instance);
	if (cwmp_get_parameter_values(vcf_name_parameter, &vcf_parameters) != NULL) {
		CWMP_LOG(ERROR, "Not able to get the value of the parameter %s", vcf_name_parameter);
		return -1;
	}
	struct cwmp_dm_parameter *param_value;
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
	struct cwmp_dm_parameter *param_value;
	list_for_each_entry (param_value, &vlf_parameters, list) {
		*value = param_value->value ? strdup(param_value->value) : NULL;
		break;
	}
	cwmp_free_all_dm_parameter_list(&vlf_parameters);
	return 0;
}

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

int cwmp_launch_upload(struct upload *pupload, struct transfer_complete **ptransfer_complete)
{
	int error = FAULT_CPE_NO_FAULT;
	char *upload_startTime;
	struct transfer_complete *p;
	char *name = NULL;
	upload_startTime = get_time(time(NULL));
	char file_path[128] = {'\0'};

	if (pupload == NULL) {
		CWMP_LOG(ERROR, "upload %s: url is null", __FUNCTION__);
		return -1;
	}
	bkp_session_delete_upload(pupload);
	bkp_session_save();

	if (pupload->file_type == NULL) {
		CWMP_LOG(ERROR, "upload %s: url is null", __FUNCTION__);
		return -1;
	}
	if (pupload->file_type[0] == '1') {
		snprintf(file_path, sizeof(file_path), "/tmp/all_configs");
		cwmp_uci_export(file_path, UCI_STANDARD_CONFIG);
	} else if (pupload->file_type[0] == '2') {
		snprintf(file_path, sizeof(file_path), "/tmp/syslog");
		copy("/var/log/syslog", file_path);
	} else if (pupload->file_type[0] == '3') {
		lookup_vcf_name(pupload->f_instance, &name);
		if (name && strlen(name) > 0) {
			snprintf(file_path, sizeof(file_path), "/tmp/%s", name);
			cwmp_uci_export_package(name, file_path, UCI_STANDARD_CONFIG);
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

	int ret = upload_file(file_path, pupload->url, pupload->username, pupload->password);
	if (ret == 200 || ret == 204)
		error = FAULT_CPE_NO_FAULT;
	else
		error = FAULT_CPE_UPLOAD_FAILURE;
	remove(file_path);

end_upload:
	p = calloc(1, sizeof(struct transfer_complete));
	if (p == NULL) {
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

void *thread_cwmp_rpc_cpe_upload(void *v)
{
	struct cwmp *cwmp = (struct cwmp *)v;
	struct upload *pupload;
	struct timespec upload_timeout = { 0, 0 };
	time_t current_time, stime;
	int error = FAULT_CPE_NO_FAULT;
	struct transfer_complete *ptransfer_complete;
	long int time_of_grace = 3600, timeout;

	for (;;) {

		if (thread_end) {
			break;
		}

		if (list_upload.next != &(list_upload)) {
			pupload = list_entry(list_upload.next, struct upload, list);
			stime = pupload->scheduled_time;
			current_time = time(NULL);
			if (pupload->scheduled_time != 0)
				timeout = current_time - pupload->scheduled_time;
			else
				timeout = 0;
			if ((timeout >= 0) && (timeout > time_of_grace)) {
				pthread_mutex_lock(&mutex_upload);
				bkp_session_delete_upload(pupload);
				ptransfer_complete = calloc(1, sizeof(struct transfer_complete));
				if (ptransfer_complete != NULL) {
					error = FAULT_CPE_DOWNLOAD_FAILURE;

					ptransfer_complete->command_key = strdup(pupload->command_key);
					ptransfer_complete->start_time = strdup(get_time(time(NULL)));
					ptransfer_complete->complete_time = strdup(ptransfer_complete->start_time);
					ptransfer_complete->fault_code = error;
					ptransfer_complete->type = TYPE_UPLOAD;
					bkp_session_insert_transfer_complete(ptransfer_complete);
					cwmp_root_cause_transfer_complete(cwmp, ptransfer_complete);
				}
				list_del(&(pupload->list));
				if (pupload->scheduled_time != 0)
					count_download_queue--;
				cwmp_free_upload_request(pupload);
				pthread_mutex_unlock(&mutex_upload);
				continue;
			}
			if ((timeout >= 0) && (timeout <= time_of_grace)) {
				pthread_mutex_lock(&(cwmp->mutex_session_send));
				CWMP_LOG(INFO, "Launch upload file %s", pupload->url);
				error = cwmp_launch_upload(pupload, &ptransfer_complete);
				if (error != FAULT_CPE_NO_FAULT) {
					bkp_session_insert_transfer_complete(ptransfer_complete);
					bkp_session_save();
					cwmp_root_cause_transfer_complete(cwmp, ptransfer_complete);
					bkp_session_delete_transfer_complete(ptransfer_complete);
				} else {
					bkp_session_delete_transfer_complete(ptransfer_complete);
					ptransfer_complete->fault_code = error;
					bkp_session_insert_transfer_complete(ptransfer_complete);
					bkp_session_save();
					cwmp_root_cause_transfer_complete(cwmp, ptransfer_complete);
				}
				pthread_mutex_unlock(&(cwmp->mutex_session_send));
				pthread_cond_signal(&(cwmp->threshold_session_send));
				pthread_mutex_lock(&mutex_upload);
				list_del(&(pupload->list));
				if (pupload->scheduled_time != 0)
					count_download_queue--;
				cwmp_free_upload_request(pupload);
				pthread_mutex_unlock(&mutex_upload);
				continue;
			}
			pthread_mutex_lock(&mutex_upload);
			upload_timeout.tv_sec = stime;
			pthread_cond_timedwait(&threshold_upload, &mutex_upload, &upload_timeout);
			pthread_mutex_unlock(&mutex_upload);
		} else {
			pthread_mutex_lock(&mutex_upload);
			pthread_cond_wait(&threshold_upload, &mutex_upload);
			pthread_mutex_unlock(&mutex_upload);
		}
	}
	return NULL;
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
	pthread_mutex_lock(&mutex_upload);
	while (list_upload.next != &(list_upload)) {
		struct upload *upload;
		upload = list_entry(list_upload.next, struct upload, list);
		list_del(&(upload->list));
		bkp_session_delete_upload(upload);
		if (upload->scheduled_time != 0)
			count_download_queue--;
		cwmp_free_upload_request(upload);
	}
	pthread_mutex_unlock(&mutex_upload);

	return CWMP_OK;
}
