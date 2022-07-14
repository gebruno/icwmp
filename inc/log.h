/*
 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	Copyright (C) 2013-2019 iopsys Software Solutions AB
 *	  Author Mohamed Kallel <mohamed.kallel@pivasoftware.com>
 *	  Author Ahmed Zribi <ahmed.zribi@pivasoftware.com>
 *
 */

#ifndef _LOG_H_
#define _LOG_H_

enum log_severity_enum
{
	EMERG,
	ALERT,
	CRITIC,
	ERROR,
	WARNING,
	NOTICE,
	INFO,
	DEBUG
};

enum log_xmlmsg_enum
{
	XML_MSG_IN,
	XML_MSG_OUT
};

void puts_log(int severity, const char *fmt, ...);
void puts_log_xmlmsg(int severity, char *msg, int msgtype);
int log_set_log_file_name(char *value);
int log_set_file_max_size(char *value);
int log_set_on_console(char *value);
int log_set_on_file(char *value);
int log_set_on_syslog(char *value);
int log_set_severity_idx(char *value);
#define DEFAULT_LOG_FILE_SIZE 10240
#define DEFAULT_LOG_FILE_NAME "/var/log/icwmpd.log"
#define DEFAULT_LOG_SEVERITY INFO

#define CWMP_LOG(SEV, MESSAGE, args...) puts_log(SEV, MESSAGE, ##args);
#define CWMP_LOG_XML_MSG puts_log_xmlmsg

#endif /* _LOG_H_ */
