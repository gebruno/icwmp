/*
 * cwmp_http.c: Utility functions for http server and client
 *
 * Copyright (C) 2022 iopsys Software Solutions AB. All rights reserved.
 *
 * Author: Mohamed Kallel <mohamed.kallel@pivasoftware.com>
 * Author: Ahmed Zribi <ahmed.zribi@pivasoftware.com>
 * Author: Omar Kallel <omar.kallel@pivasoftware.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#ifndef CWMP_HTTP_H__
#define CWMP_HTTP_H__

#include "common.h"

void cwmp_http_server_init(void);
void cwmp_http_server_listen(void);

int cwmp_http_client_init(struct cwmp *cwmp);
int cwmp_http_send_message(struct cwmp *cwmp, char *msg_out, int msg_out_len, char **msg_in);
void cwmp_http_client_exit(void);

void cwmp_http_remove_cookies_file(void);
void cwmp_http_set_timeout(void);

#endif /* CWMP_HTTP_H__ */
