/*
 * heartbeat.h - CWMP HeartBeat mechanism
 *
 * Copyright (C) 2022, IOPSYS Software Solutions AB.
 *
 *	  Author Omar Kallel <omar.kallel@pivasoftware.com>
 *
 * See LICENSE file for license related information.
 *
 */
#ifndef HEARTBEAT_H
#define HEARTBEAT_H

#include <stdbool.h>
extern struct uloop_timeout heartbeat_session_timer;
extern int heart_beat_retry_count_session;
extern bool old_heartbeat_enable;

void cwmp_heartbeat_session_timer(struct uloop_timeout *timeout);
void intiate_heartbeat_procedures();
void reinit_heartbeat_procedures();
#endif
