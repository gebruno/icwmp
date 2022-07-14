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
extern pthread_mutex_t mutex_heartbeat;
extern pthread_mutex_t mutex_heartbeat_session;
extern pthread_cond_t threshold_heartbeat_session;
extern pthread_cond_t threasheld_retry_session;
extern int heart_beat_retry_count_session;
extern bool old_heartbeat_enable;

void *thread_heartbeat_session(void *v);
void check_trigger_heartbeat_session();
#endif
