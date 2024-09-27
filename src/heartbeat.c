/*
 * heartbeat.c - CWMP HeartBeat mechanism
 *
 * Copyright (C) 2022, IOPSYS Software Solutions AB.
 *
 *	  Author Omar Kallel <omar.kallel@pivasoftware.com>
 *
 * See LICENSE file for license related information.
 *
 */
#include <pthread.h>
#include <unistd.h>
#include <libubox/uloop.h>

#include "heartbeat.h"
#include "common.h"
#include "config.h"
#include "session.h"
#include "backupSession.h"
#include "log.h"
#include "event.h"
#include "http.h"
#include "cwmp_event.h"

static void cwmp_heartbeat_session_timer(struct uloop_timeout *timeout);

struct uloop_timeout heartbeat_session_timer = { .cb = cwmp_heartbeat_session_timer };

long int cwmp_heartbeat_session_time(void)
{
	long int heartbeat_report;
	time_t now = time(NULL);
	struct tm *now_tm = gmtime((const time_t *)&now);
	struct tm *heart_time = gmtime((const time_t *)&cwmp_ctx.conf.heart_time);
	struct tm heart_init_tm = {.tm_year = now_tm->tm_year, .tm_mon = now_tm->tm_mon, .tm_mday = now_tm->tm_mday, .tm_hour = heart_time->tm_hour, .tm_min = heart_time->tm_min, .tm_sec = heart_time->tm_sec};
	time_t heart_init_time = mktime(&heart_init_tm);
	if (heart_init_time - mktime(now_tm) < 0) {
		add_day_to_time(&heart_init_tm);
		heart_init_time = mktime(&heart_init_tm);
	}

	heartbeat_report = heart_init_time - mktime(now_tm);

	return  heartbeat_report;
}

void cwmp_heartbeat_session_timer(struct uloop_timeout *timeout  __attribute__((unused)))
{
	if (cwmp_ctx.conf.heart_beat_enable) {
		//HEARTBEAT event must wait a Non-HEARTBEAT Inform is being retried to be completed
		if (cwmp_ctx.session->session_status.last_status == SESSION_FAILURE) {
			cwmp_ctx.session->session_status.next_heartbeat = true;
			cwmp_ctx.session->session_status.is_heartbeat = false;
			return;
		}
		//struct session_timer_event *heartbeat_inform_event = calloc(1, sizeof(struct session_timer_event));

		uloop_timeout_set(&heartbeat_session_timer, cwmp_ctx.conf.heartbeat_interval * 1000);

		cwmp_ctx.session->session_status.next_heartbeat = false;
		cwmp_ctx.session->session_status.is_heartbeat = true;
		cwmp_add_event_container(EVENT_IDX_14HEARTBEAT, "");
		start_cwmp_session();
	}
}

void intiate_heartbeat_procedures()
{
	uloop_timeout_cancel(&heartbeat_session_timer);
	if (cwmp_ctx.conf.heart_beat_enable) {
		if (cwmp_ctx.conf.heart_time == 0) {
			uloop_timeout_set(&heartbeat_session_timer, cwmp_ctx.conf.heartbeat_interval * 1000);
		} else {
			time_t hearttime_interval = cwmp_ctx.conf.heart_time - time(NULL);
			if (hearttime_interval >= 0) {
				uloop_timeout_set(&heartbeat_session_timer, hearttime_interval * 1000);
			} else {
				uloop_timeout_set(&heartbeat_session_timer, cwmp_heartbeat_session_time() * 1000);
			}
		}
	}
}

void reinit_heartbeat_procedures()
{
	if (cwmp_ctx.conf.heart_beat_enable) {
		if (!cwmp_ctx.prev_heartbeat_enable || (cwmp_ctx.prev_heartbeat_interval != cwmp_ctx.conf.heartbeat_interval) || (cwmp_ctx.prev_heartbeat_time != cwmp_ctx.conf.heart_time)) {
			cwmp_ctx.heart_session = true;
			if ((cwmp_ctx.prev_heartbeat_time != cwmp_ctx.conf.heart_time) && cwmp_ctx.conf.heart_time != 0) {
				time_t hearttime_interval = cwmp_ctx.conf.heart_time - time(NULL);
				if (hearttime_interval >= 0)
					cwmp_ctx.heart_session_interval = hearttime_interval;
				else
					cwmp_ctx.heart_session_interval = cwmp_heartbeat_session_time();
			} else
				cwmp_ctx.heart_session_interval = cwmp_ctx.conf.heartbeat_interval;
		}
	} else
		uloop_timeout_cancel(&heartbeat_session_timer);

	cwmp_ctx.prev_heartbeat_enable = cwmp_ctx.conf.heart_beat_enable;
	cwmp_ctx.prev_heartbeat_interval = cwmp_ctx.conf.heartbeat_interval;
	cwmp_ctx.prev_heartbeat_time = cwmp_ctx.conf.heart_time;
}
