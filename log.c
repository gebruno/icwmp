/*
 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	Copyright (C) 2013-2021 iopsys Software Solutions AB
 *	  Author Mohamed Kallel <mohamed.kallel@pivasoftware.com>
 *	  Author Ahmed Zribi <ahmed.zribi@pivasoftware.com>
 *
 */

#include <syslog.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>

#include "log.h"
#include "common.h"

static char *SEVERITY_NAMES[8] = { "[EMERG]  ", "[ALERT]  ", "[CRITIC] ", "[ERROR]  ", "[WARNING]", "[NOTICE] ", "[INFO]   ", "[DEBUG]  " };
static int log_severity = DEFAULT_LOG_SEVERITY;
static long int log_max_size = DEFAULT_LOG_FILE_SIZE;
static char log_file_name[256];
static bool enable_log_file = true;
static bool enable_log_stdout = false;
static bool enable_log_syslog = true;

#ifdef CWMP_ENABLE_FILE_LOGGING
static pthread_mutex_t mutex_log = PTHREAD_MUTEX_INITIALIZER;
#endif

int log_set_severity_idx(char *value)
{
	if (value == NULL)
		return 1;

	int i;
	for (i = 0; i < 8; i++) {
		if (strstr(SEVERITY_NAMES[i], value) != NULL) {
			log_severity = i;
			return 0;
		}
	}
	return 1;
}

int log_set_log_file_name(char *value)
{
	if (value != NULL) {
		CWMP_STRNCPY(log_file_name, value, sizeof(log_file_name));
	} else {
		CWMP_STRNCPY(log_file_name, DEFAULT_LOG_FILE_NAME, sizeof(log_file_name));
	}
	return 1;
}

int log_set_file_max_size(char *value)
{
	if (value != NULL) {
		log_max_size = atol(value);
	} else {
		log_max_size = 102400;
	}
	return 1;
}

int log_set_on_console(char *value)
{
	if (value == NULL)
		return 1;

	enable_log_stdout = uci_str_to_bool(value);
	return 1;
}

int log_set_on_file(char *value)
{
	if (value == NULL)
		return 1;

	enable_log_file = uci_str_to_bool(value);
	return 1;
}

int log_set_on_syslog(char *value)
{
	if (value == NULL)
		return 1;

	enable_log_syslog = uci_str_to_bool(value);
	return 1;
}

#ifdef CWMP_ENABLE_FILE_LOGGING
static void log_to_file(const char *file_name, const char *msg, int severity, int *xml_msgtype)
{
	struct tm *Tm;
	struct timeval tv;
	char log_file_name_bak[258] = {0};
	int i;
	FILE *pLog = NULL;
	struct stat st;
	long int size = 0;
	char buf[1024] = {0};
	char buf_file[1024] = {0};
	char *description = NULL, *separator = NULL;

	pthread_mutex_lock(&mutex_log);
	gettimeofday(&tv, 0);
	Tm = localtime(&tv.tv_sec);
	i = snprintf(buf, sizeof(buf), "%02d-%02d-%4d, %02d:%02d:%02d %s ", Tm->tm_mday, Tm->tm_mon + 1, Tm->tm_year + 1900, Tm->tm_hour, Tm->tm_min, Tm->tm_sec, SEVERITY_NAMES[severity]);

	if (xml_msgtype != NULL) {
		if (*xml_msgtype == XML_MSG_IN) {
			description = "MESSAGE IN\n";
			separator = "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n";
		} else {
			description = "MESSAGE OUT\n";
			separator = ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n";
		}
	} else {
		int rem_len = sizeof(buf) - i;
		snprintf(buf + i, rem_len, "%s", msg);
	}

	if (enable_log_file) {
		if (stat(file_name, &st) == 0) {
			size = st.st_size;
		}
		if (size >= log_max_size) {
			snprintf(log_file_name_bak, sizeof(log_file_name_bak), "%s.1", file_name);
			rename(file_name, log_file_name_bak);
			pLog = fopen(file_name, "w");
		} else {
			pLog = fopen(file_name, "a+");
		}

		if (xml_msgtype != NULL) {
			fputs(buf, pLog);
			fputs(description, pLog);
			fputs(separator, pLog);
			fputs(msg, pLog);
			fputs("\n", pLog);
			fputs(separator, pLog);
			fclose(pLog);
		}else {
			CWMP_STRNCPY(buf_file, buf, sizeof(buf_file));
			buf_file[strlen(buf)] = '\n';
			buf_file[strlen(buf) + 1] = '\0';
			fputs(buf_file, pLog);
			fclose(pLog);
		}
	}

	if (enable_log_stdout) {
		puts(buf);
		if (xml_msgtype != NULL) {
			puts(description);
			puts(separator);
			puts(msg);
			puts("\n");
			puts(separator);
		}
	}

	pthread_mutex_unlock(&mutex_log);
}
#endif

void puts_log(int severity, const char *fmt, ...)
{
	va_list args;

	if (severity > log_severity) {
		return;
	}

	if (enable_log_syslog) {
		va_start(args, fmt);
		vsyslog(severity, fmt, args);
		va_end(args);
	}

#ifdef CWMP_ENABLE_FILE_LOGGING
	char buf[1024] = {0};
	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);
	if (strlen(log_file_name) == 0) {
		log_to_file(DEFAULT_LOG_FILE_NAME, (const char*)buf, severity, NULL);
	} else {
		log_to_file(log_file_name, (const char*)buf, severity, NULL);
	}
#endif
}

void puts_log_xmlmsg(int severity, char *msg, int msgtype)
{
	if (severity > log_severity) {
		return;
	}

	if (enable_log_syslog) {
		syslog(severity, "%s: %s", ((msgtype == XML_MSG_IN) ? "IN" : "OUT"), msg);
		if (1024 < strlen(msg))
			syslog(severity, "Truncated message at %zu characters", strlen(msg));
	}

#ifdef CWMP_ENABLE_FILE_LOGGING
	if (strlen(log_file_name) == 0) {
		log_to_file(DEFAULT_LOG_FILE_NAME, (const char*)msg, severity, &msgtype);
	} else {
		log_to_file(log_file_name, (const char *)msg, severity, &msgtype);
	}
#endif
}
