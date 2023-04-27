/*
 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	Copyright (C) 2013-2021 iopsys Software Solutions AB
 *	  Author Mohamed Kallel <mohamed.kallel@pivasoftware.com>
 *	  Author Ahmed Zribi <ahmed.zribi@pivasoftware.com>
 *	  Author Omar Kallel <omar.kallel@pivasoftware.com>
 *	  Copyright (C) 2011-2012 Luka Perkov <freecwmp@lukaperkov.net>
 */
#include <curl/curl.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "http.h"
#include "cwmp_uci.h"
#include "log.h"
#include "event.h"
#include "ubus_utils.h"
#include "config.h"
#include "digauth.h"

#define REALM "authenticate@cwmp"
#define OPAQUE "11733b200778ce33060f31c9af70a870ba96ddd4"
#define HTTP_GET_HDR_LEN 512
#define HTTP_FD_FEEDS_COUNT 10 /* Maximum number of lines to be read from HTTP header */

static struct http_client http_c;
static bool curl_glob_init = false;
static CURL *curl = NULL;

char *fc_cookies = "/tmp/icwmp_cookies";

void http_set_timeout(void)
{
	if (curl)
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 1);
}

int http_client_init(struct cwmp *cwmp)
{
	char *dhcp_dis = NULL;
	char *acs_var_stat = NULL;

	uci_get_value(UCI_DHCP_DISCOVERY_PATH, &dhcp_dis);
	char *url = NULL;
	global_string_param_read(&cwmp->conf.acsurl, &url);

	if (dhcp_dis && cwmp->retry_count_session > 0 && strcmp(dhcp_dis, "enable") == 0) {
		uci_get_state_value(UCI_DHCP_ACS_URL, &acs_var_stat);
		if (acs_var_stat) {
			if (icwmp_asprintf(&http_c.url, "%s", acs_var_stat) == -1) {
				free(acs_var_stat);
				FREE(dhcp_dis);
				FREE(url);
				return -1;
			}
		} else {
			if (CWMP_STRLEN(url) == 0 || icwmp_asprintf(&http_c.url, "%s", url) == -1) {
				FREE(dhcp_dis);
				FREE(url);
				return -1;
			}
		}
	} else {
		if (url == NULL || icwmp_asprintf(&http_c.url, "%s", url) == -1) {
			FREE(dhcp_dis);
			FREE(url);
			return -1;
		}
	}
	FREE(url);

	if (dhcp_dis)
		free(dhcp_dis);

	CWMP_LOG(INFO, "ACS url: %s", http_c.url);

	/* TODO debug ssl config from freecwmp*/

	curl_global_init(CURL_GLOBAL_SSL);
	curl_glob_init = true;
	curl = curl_easy_init();
	if (!curl)
		return -1;

	bool v6_enable = global_bool_param_read(&cwmp->conf.ipv6_enable);
	if (v6_enable) {
		unsigned char buf[sizeof(struct in6_addr)];

		char *ip = NULL;
		curl_easy_setopt(curl, CURLOPT_URL, http_c.url);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, HTTP_TIMEOUT);
		curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
		curl_easy_getinfo(curl, CURLINFO_PRIMARY_IP, &ip);
		curl_easy_perform(curl);
		int tmp = inet_pton(AF_INET, ip, buf);

		cwmp_uci_set_value("cwmp", "acs", "ip_version", (tmp == 1) ? "4" : "6");
	}
	return 0;
}

void http_client_exit(void)
{
	icwmp_free(http_c.url);

	if (http_c.header_list) {
		curl_slist_free_all(http_c.header_list);
		http_c.header_list = NULL;
	}
	if (file_exists(fc_cookies))
		remove(fc_cookies);
	if (curl) {
		curl_easy_cleanup(curl);
		curl = NULL;
	}

	if (curl_glob_init) {
		curl_global_cleanup();
		curl_glob_init = false;
	}
}

static size_t http_get_response(void *buffer, size_t size, size_t rxed, char **msg_in)
{
	char *c = NULL;

	CWMP_LOG(INFO, "HTTP CURL handler function");

	if (msg_in == NULL) {
		CWMP_LOG(ERROR, "msg_in is null");
		return 0;
	}

	if (buffer == NULL) {
		CWMP_LOG(ERROR, "Buffer is null");
		return 0;
	}

	if (cwmp_asprintf(&c, "%s%.*s", *msg_in, (int)(size * rxed), (char *)buffer) == -1) {
		FREE(*msg_in);
		return -1;
	}

	FREE(*msg_in);
	*msg_in = c;

	return size * rxed;
}

int http_send_message(struct cwmp *cwmp, char *msg_out, int msg_out_len, char **msg_in)
{
	unsigned char buf[sizeof(struct in6_addr)];
	int tmp = 0;
	CURLcode res;
	long http_code = 0;
	static char ip_acs[128] = { 0 };
	char *ip = NULL, *temp = NULL;
	char errbuf[CURL_ERROR_SIZE];

	http_c.header_list = NULL;
	http_c.header_list = curl_slist_append(http_c.header_list, "User-Agent: iopsys-cwmp");
	if (!http_c.header_list)
		return -1;
	http_c.header_list = curl_slist_append(http_c.header_list, "Content-Type: text/xml");
	if (!http_c.header_list)
		return -1;

	if (global_bool_param_read(&cwmp->conf.http_disable_100continue)) {
		http_c.header_list = curl_slist_append(http_c.header_list, "Expect:");
		if (!http_c.header_list)
			return -1;
	}
	curl_easy_setopt(curl, CURLOPT_URL, http_c.url);
	global_string_param_read(&cwmp->conf.acs_userid, &temp);
	curl_easy_setopt(curl, CURLOPT_USERNAME, temp);
	FREE(temp);
	global_string_param_read(&cwmp->conf.acs_passwd, &temp);
	curl_easy_setopt(curl, CURLOPT_PASSWORD, temp);
	FREE(temp);
	curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC | CURLAUTH_DIGEST);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, HTTP_TIMEOUT);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, HTTP_TIMEOUT);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_POSTREDIR, CURL_REDIR_POST_ALL);
	curl_easy_setopt(curl, CURLOPT_NOBODY, 0);
	switch (global_int_param_read(&cwmp->conf.compression)) {
	case COMP_NONE:
		break;
	case COMP_GZIP:
		curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip");
		http_c.header_list = curl_slist_append(http_c.header_list, "Content-Encoding: gzip");
		break;
	case COMP_DEFLATE:
		curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "deflate");
		http_c.header_list = curl_slist_append(http_c.header_list, "Content-Encoding: deflate");
		break;
	}
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, http_c.header_list);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, msg_out);
	if (msg_out)
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)msg_out_len);
	else
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0);

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_get_response);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, msg_in);

#ifdef DEVEL
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
#endif
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

	curl_easy_setopt(curl, CURLOPT_COOKIEFILE, fc_cookies);
	curl_easy_setopt(curl, CURLOPT_COOKIEJAR, fc_cookies);

	global_string_param_read(&cwmp->conf.acs_ssl_capath, &temp);
	if (CWMP_STRLEN(temp) != 0)
		curl_easy_setopt(curl, CURLOPT_CAPATH, temp);
	if (global_bool_param_read(&cwmp->conf.insecure_enable)) {
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
	}
	FREE(temp);

	global_string_param_read(&cwmp->conf.interface, &temp);
	curl_easy_setopt(curl, CURLOPT_INTERFACE, temp);
	FREE(temp);
	*msg_in = (char *)calloc(1, sizeof(char));

	res = curl_easy_perform(curl);

	if (res != CURLE_OK) {
		size_t len = strlen(errbuf);
		if (len) {
			if (errbuf[len - 1] == '\n')
				errbuf[len - 1] = '\0';
			CWMP_LOG(ERROR, "libcurl: (%d) %s", res, errbuf);
		} else {
			CWMP_LOG(ERROR, "libcurl: (%d) %s", res, curl_easy_strerror(res));
		}
	}

	if (*msg_in && !strlen(*msg_in))
		FREE(*msg_in);

	curl_easy_getinfo(curl, CURLINFO_PRIMARY_IP, &ip);
	if (ip && ip[0] != '\0') {
		if (ip_acs[0] == '\0' || strcmp(ip_acs, ip) != 0) {
			CWMP_STRNCPY(ip_acs, ip, sizeof(ip_acs));
			tmp = inet_pton(AF_INET, ip, buf);
			if (tmp == 1)
				tmp = 0;
			else
				tmp = inet_pton(AF_INET6, ip, buf);

			cwmp_uci_set_varstate_value("cwmp", "acs", tmp ? "ip6" : "ip", ip_acs);

			// Trigger firewall to reload firewall.cwmp
			struct blob_buf b = { 0 };
			memset(&b, 0, sizeof(struct blob_buf));
			blob_buf_init(&b, 0);
			bb_add_string(&b, "config", "firewall");
			icwmp_ubus_invoke("uci", "commit", b.head, NULL, NULL);
			blob_buf_free(&b);
		}
	}

	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
	if (http_code == 204) {
		CWMP_LOG(INFO, "Receive HTTP 204 No Content");
	}

	if (http_code == 415) {
		global_int_param_write(&cwmp->conf.compression, COMP_NONE);
		goto error;
	}
	if (http_code != 200 && http_code != 204)
		goto error;

	/* TODO add check for 301, 302 and 307 HTTP Redirect*/

	if (http_c.header_list) {
		curl_slist_free_all(http_c.header_list);
		http_c.header_list = NULL;
	}

	if (res)
		goto error;

	return 0;

error:
	FREE(*msg_in);
	if (http_c.header_list) {
		curl_slist_free_all(http_c.header_list);
		http_c.header_list = NULL;
	}
	return -1;
}

void http_success_cr(void)
{
	CWMP_LOG(INFO, "Connection Request thread: add connection request event in the queue");
	pthread_mutex_lock(&(cwmp_main.mutex_session_queue));
	cwmp_add_event_container(&cwmp_main, EVENT_IDX_6CONNECTION_REQUEST, "");
	pthread_mutex_unlock(&(cwmp_main.mutex_session_queue));
	pthread_cond_signal(&(cwmp_main.threshold_session_send));
}

static void http_cr_new_client(int client, bool service_available)
{
	FILE *fp = NULL;
	char data[BUFSIZ] = {0};
	char buffer[BUFSIZ] = {0};
	char auth_digest_buffer[BUFSIZ] = {0};
	int8_t auth_status = 0;
	bool auth_digest_checked = false;
	bool method_is_get = false;
	bool internal_error = false;
	char cr_http_get_head[HTTP_GET_HDR_LEN] = {0};
	char *temp = NULL;
	char *username = NULL;
	char *password = NULL;
	fd_set rfds;
	struct timeval tv;
	int fd_feed = 0;
	int status = 0;

	CWMP_LOG(INFO, "Received a new CR from ACS, service_available: %d", service_available);

	global_string_param_read(&cwmp_main.conf.cpe_userid, &username);
	global_string_param_read(&cwmp_main.conf.cpe_passwd, &password);

	memset(auth_digest_buffer, 0, BUFSIZ);
	if (!username || !password) {
		// if we dont have username or password configured proceed with connecting to ACS
		CWMP_LOG(INFO, "Failed to get acs username and password");
		service_available = false;
		goto http_end;
	}

	global_string_param_read(&cwmp_main.conf.connection_request_path, &temp);
	snprintf(cr_http_get_head, sizeof(cr_http_get_head), "GET %s HTTP/1.1", temp);
	FREE(temp);

	/* Initialize timeout of select, so that it will wait for specific time
	 * period before timed out to receive data from client. Otherwise if
	 * client will not send any data after a successful connection then server
	 * will wait forever and not entertain any other connection requests
	 */
	tv.tv_sec = global_int_param_read(&cwmp_main.conf.cr_timeout);
	tv.tv_usec = 0;
	FD_ZERO(&rfds);
	FD_SET(client, &rfds);

	/* Make FD non blocking, so that no operation on FD will block and make the
	 * server halt forever.
	 */
	if (fcntl(client, F_SETFL, O_NONBLOCK) < 0) {
		CWMP_LOG(ERROR, "Failed to set NONBLOCK");
		goto http_end;
	}

	fp = fdopen(client, "r+");
	if (fp == NULL) {
		CWMP_LOG(ERROR, "Failed to open client socket");
		goto http_end;
	}

	bool read_done = false;
	/* Perform read from FD until all required data are collected or
	 * HTTP_FD_FEEDS_COUNT number of read operation has been performed.
	 * So that flooding of data not blocks the server.
	 */
	while (!read_done && fd_feed < HTTP_FD_FEEDS_COUNT) {
		status = select(client+1, &rfds, NULL, NULL, &tv);
		if (status <= 0) {
			CWMP_LOG(INFO, "TIMEOUT occurred or select failed");
			break;
		}

		/* Check how many bytes available in the FD */
		int read_bytes = 0;
		if (ioctl(client, FIONREAD, &read_bytes) == -1) {
			CWMP_LOG(INFO, "ioctl failed");
			break;
		}

		if (read_bytes == 0) {
			/* it means the client has been disconnected */
			CWMP_LOG(INFO, "client disconnected");
			break;
		}

		/* Read upto the number of bytes or HTTP_FD_FEEDS_COUNT number of
		 * read operation whichever is earlier, to avoid halt on data flooding
		 */
		while (read_bytes > 0 && fd_feed < HTTP_FD_FEEDS_COUNT) {
			if (fgets(buffer, sizeof(buffer), fp) == NULL) {
				CWMP_LOG(INFO, "No more data from FD");
				break;
			}

			size_t buf_len = strlen(buffer);
			read_bytes = read_bytes - buf_len;
			fd_feed = fd_feed + 1;

			/* Check if a whole line has been read, since a non blocking FD so
			 * possible to have fewer bytes than its in whole line based on the
			 * availability of data in the FD
			 */
			if (buffer[buf_len - 1] != '\n') {
				/* there should be more data in current line, store the current
				 * data and wait for next read if max data length not exceeded
				 */
				size_t avail_space = (size_t)(sizeof(data) - strlen(data));
				if (buf_len < avail_space) {
					strcat(data, buffer);
					continue;
				}
			} else {
				/* A whole line has been read, so store it if max data length is
				 * not exceeded and process the data
				 */
				size_t avail_space = (size_t)(sizeof(data) - strlen(data));
				if (buf_len < avail_space) {
					strcat(data, buffer);
				}
			}

			strip_lead_trail_char(data, '\r');
			strip_lead_trail_char(data, '\n');
			if (strlen(data) == 0) {
				/* empty line reached */
				CWMP_LOG(DEBUG, "Empty line found in packet");
				read_done = true;
				break;
			}

			if (fd_feed == 1 && (strstr(data, "GET ") == NULL || strstr(data, "HTTP/1.1") == NULL)) {
				CWMP_LOG(INFO, "GET not found at initial:: %s", data);
				read_done = true;
				break;
			}

			CWMP_LOG(DEBUG, "Data:: %s", data);
			if (strstr(data, "GET ") != NULL && strstr(data, "HTTP/1.1") != NULL) {
				// check if extra url parameter then ignore extra params
				int j = 0;
				bool ignore = false;
				char rec_http_get_head[HTTP_GET_HDR_LEN] = {0};

				memset(rec_http_get_head, 0, HTTP_GET_HDR_LEN);
				for (size_t i = 0; i < strlen(data) && j < (HTTP_GET_HDR_LEN - 1); i++) {
					if (data[i] == '?')
						ignore = true;
					if (data[i] == ' ')
						ignore = false;
					if (ignore == false) {
						rec_http_get_head[j] = data[i];
						j++;
					}
				}

				if (!strncasecmp(rec_http_get_head, cr_http_get_head, strlen(cr_http_get_head)))
					method_is_get = true;
			}

			if (!strncasecmp(data, "Authorization: Digest ", strlen("Authorization: Digest "))) {
				auth_digest_checked = true;
				CWMP_STRNCPY(auth_digest_buffer, data, BUFSIZ);
			}

			memset(data, 0, sizeof(data));
		}
	}

	if (!service_available || !method_is_get) {
		goto http_end;
	}

	int auth_check = validate_http_digest_auth("GET", "/", auth_digest_buffer + strlen("Authorization: Digest "), REALM, username, password, 300);
	if (auth_check == -1) { /* invalid nonce */
		internal_error = true;
		goto http_end;
	}
	if (auth_digest_checked && auth_check == 1)
		auth_status = 1;
	else
		auth_status = 0;
http_end:
	FREE(username);
	FREE(password);

	if (fp) {
		fflush(fp);
	}

	if (!service_available || !method_is_get) {
		CWMP_LOG(WARNING, "Receive Connection Request: Return 503 Service Unavailable");
		if (fp) {
			fputs("HTTP/1.1 503 Service Unavailable\r\n", fp);
			fputs("Connection: close\r\n", fp);
			fputs("Content-Length: 0\r\n", fp);
		}
	} else if (auth_status) {
		CWMP_LOG(INFO, "Receive Connection Request: success authentication");
		if (fp) {
			fputs("HTTP/1.1 200 OK\r\n", fp);
			fputs("Connection: close\r\n", fp);
			fputs("Content-Length: 0\r\n", fp);
		}
		http_success_cr();
	} else if (internal_error) {
		CWMP_LOG(WARNING, "Receive Connection Request: Return 500 Internal Error");
		if (fp) {
			fputs("HTTP/1.1 500 Internal Server Error\r\n", fp);
			fputs("Connection: close\r\n", fp);
			fputs("Content-Length: 0\r\n", fp);
		}
	}
	else {
		CWMP_LOG(WARNING, "Receive Connection Request: Return 401 Unauthorized");
		if (fp) {
			fputs("HTTP/1.1 401 Unauthorized\r\n", fp);
			fputs("Connection: close\r\n", fp);
			http_authentication_failure_resp(fp, "GET", "/", REALM, OPAQUE);
			fputs("\r\n", fp);
		}
	}
	if (fp) {
		fputs("\r\n", fp);
		fclose(fp);
	}
}

void http_server_init(void)
{
	struct sockaddr_in6 server = { 0 };
	unsigned short cr_port;
	unsigned short prev_cr_port = (unsigned short)global_int_param_read(&cwmp_main.conf.connection_request_port);

	for (;;) {
		cr_port = (unsigned short)global_int_param_read(&cwmp_main.conf.connection_request_port);
		unsigned short i = (DEFAULT_CONNECTION_REQUEST_PORT == cr_port) ? 1 : 0;
		//Create socket
		if (thread_end)
			return;

		cwmp_main.cr_socket_desc = socket(AF_INET6, SOCK_STREAM, 0);
		if (cwmp_main.cr_socket_desc == -1) {
			CWMP_LOG(ERROR, "Could not open server socket for Connection Requests, Error no is : %d, Error description is : %s", errno, strerror(errno));
			sleep(1);
			continue;
		}

		fcntl(cwmp_main.cr_socket_desc, F_SETFD, fcntl(cwmp_main.cr_socket_desc, F_GETFD) | FD_CLOEXEC);

		int reusaddr = 1;
		if (setsockopt(cwmp_main.cr_socket_desc, SOL_SOCKET, SO_REUSEADDR, &reusaddr, sizeof(int)) < 0) {
			CWMP_LOG(WARNING, "setsockopt(SO_REUSEADDR) failed");
		}

		//Prepare the sockaddr_in structure
		server.sin6_family = AF_INET6;
		server.sin6_addr = in6addr_any;

		for (;; i++) {
			if (thread_end)
				return;

			server.sin6_port = htons(cr_port);
			//Bind
			if (bind(cwmp_main.cr_socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0) {
				//print the error message
				CWMP_LOG(ERROR, "Could not bind server socket on the port %d, Error no is : %d, Error description is : %s", cr_port, errno, strerror(errno));
				cr_port = DEFAULT_CONNECTION_REQUEST_PORT + i;
				CWMP_LOG(INFO, "Trying to use another connection request port: %d", cr_port);
				continue;
			}
			break;
		}
		break;
	}
	if (cr_port != prev_cr_port) {
		char cr_port_str[6];
		snprintf(cr_port_str, 6, "%hu", cr_port);
		cr_port_str[5] = '\0';
		cwmp_uci_set_value("cwmp", "cpe", "port", cr_port_str);
		connection_request_port_value_change(&cwmp_main, cr_port);
	}

	CWMP_LOG(INFO, "Connection Request server initiated with the port: %d", cr_port);
}

void http_server_listen(void)
{
	int c;
	int cr_request = 0;
	time_t restrict_start_time = 0;
	struct sockaddr_in6 client;

	//Listen
	listen(cwmp_main.cr_socket_desc, 3);

	//Accept and incoming connection
	c = sizeof(struct sockaddr_in);
	do {
		if (thread_end)
			return;

		int client_sock = accept(cwmp_main.cr_socket_desc, (struct sockaddr *)&client, (socklen_t *)&c);
		if (client_sock < 0) {
			CWMP_LOG(ERROR, "Could not accept connections for Connection Requests!");
			shutdown(cwmp_main.cr_socket_desc, SHUT_RDWR);
			http_server_init();
			listen(cwmp_main.cr_socket_desc, 3);
			cr_request = 0;
			restrict_start_time = 0;
			continue;
		}

		bool service_available;
		time_t current_time;

		current_time = time(NULL);
		service_available = true;
		if ((restrict_start_time == 0) || ((current_time - restrict_start_time) > CONNECTION_REQUEST_RESTRICT_PERIOD)) {
			restrict_start_time = current_time;
			cr_request = 1;
		} else {
			cr_request++;
			if (cr_request > CONNECTION_REQUEST_RESTRICT_REQUEST) {
				restrict_start_time = current_time;
				service_available = false;
				CWMP_LOG(WARNING, "CR count %d exceeded max %d, SKIPPED", cr_request, CONNECTION_REQUEST_RESTRICT_REQUEST);
			}
		}
		http_cr_new_client(client_sock, service_available);
		close(client_sock);
	} while (1);
}
