/*
 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	Copyright (C) 2021 iopsys Software Solutions AB
 *	  Author Amin Ben Ramdhane <amin.benramdhane@pivasoftware.com>
 */

#include <unistd.h>
#include "reboot.h"
#include "cwmp_uci.h"
#include "log.h"
#include "session.h"

static pthread_t delay_reboot_thread;
static pthread_t delay_schedule_thread;
static int g_curr_delay_reboot = -1;
static time_t g_curr_schedule_redoot = 0;

static void *thread_delay_reboot(void *arg)
{
	struct cwmp *cwmp = (struct cwmp *)arg;

	int delay_reboot = global_int_param_read(&cwmp->conf.delay_reboot);
	CWMP_LOG(INFO, "The device will reboot after %d seconds", delay_reboot);
	sleep(delay_reboot);
	cwmp_uci_set_value("cwmp", "cpe", "delay_reboot", "-1");
	/* check if the session is running before calling reboot method */
	/* if the session is in progress, wait until the end of the session */
	/* else calling reboot method */
	if (cwmp->session_status.last_status == SESSION_RUNNING) {
		cwmp_set_end_session(END_SESSION_REBOOT);
	} else {
		cwmp_reboot("delay_reboot");
		exit(EXIT_SUCCESS);
	}

	return NULL;
}

static void create_delay_reboot_thread(struct cwmp *cwmp, bool thread_exist)
{
	if (thread_exist) {
		CWMP_LOG(INFO, "There is already a delay reboot thread!, Cancel the current thread");

		pthread_cancel(delay_reboot_thread);
		create_delay_reboot_thread(cwmp, false);
	} else {
		CWMP_LOG(INFO, "Create a delay reboot thread");

		if (pthread_create(&delay_reboot_thread, NULL, &thread_delay_reboot, (void *)cwmp)) {
			CWMP_LOG(ERROR, "Error when creating the delay reboot thread!");
		}

		if (pthread_detach(delay_reboot_thread)) {
			CWMP_LOG(ERROR, "Error when creating the delay reboot thread!");
		}
	}
}

static void *thread_schedule_reboot(void *arg)
{
	struct cwmp *cwmp = (struct cwmp *)arg;
	time_t remaining_time = global_time_param_read(&cwmp->conf.schedule_reboot) - time(NULL);

	CWMP_LOG(INFO, "The device will reboot after %ld seconds", remaining_time);
	sleep(remaining_time);
	cwmp_uci_set_value("cwmp", "cpe", "schedule_reboot", "0001-01-01T00:00:00Z");

	/* check if the session is running before calling reboot method */
	/* if the session is in progress, wait until the end of the session */
	/* else calling reboot method */
	if (cwmp->session_status.last_status == SESSION_RUNNING) {
		cwmp_set_end_session(END_SESSION_REBOOT);
	} else {
		cwmp_reboot("schedule_reboot");
		exit(EXIT_SUCCESS);
	}

	return NULL;
}

static void create_schedule_reboot_thread(struct cwmp *cwmp, bool thread_exist)
{
	if (thread_exist) {
		CWMP_LOG(INFO, "There is already a schedule reboot thread!, Cancel the current thread");

		pthread_cancel(delay_schedule_thread);
		create_schedule_reboot_thread(cwmp, false);
	} else {
		CWMP_LOG(INFO, "Create a schedule reboot thread");

		if (pthread_create(&delay_schedule_thread, NULL, &thread_schedule_reboot, (void *)cwmp)) {
			CWMP_LOG(ERROR, "Error when creating the schedule reboot thread!");
		}

		if (pthread_detach(delay_schedule_thread)) {
			CWMP_LOG(ERROR, "Error when detaching the schedule reboot thread!");
		}
	}
}

void launch_reboot_methods(struct cwmp *cwmp)
{
	int delay_reboot = global_int_param_read(&cwmp->conf.delay_reboot);
	time_t schedule_reboot = global_time_param_read(&cwmp->conf.schedule_reboot);

	if (delay_reboot != g_curr_delay_reboot && delay_reboot > 0) {

		create_delay_reboot_thread(cwmp, (g_curr_delay_reboot != -1));
		g_curr_delay_reboot = delay_reboot;
	}

	if (schedule_reboot != g_curr_schedule_redoot && (schedule_reboot - time(NULL)) > 0) {

		create_schedule_reboot_thread(cwmp, (g_curr_schedule_redoot != 0));
		g_curr_schedule_redoot = schedule_reboot;
	}
}
