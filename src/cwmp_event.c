/*
 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	Copyright (C) 2013-2021 iopsys Software Solutions AB
 *	  Author Omar Kallel <omar.kallel@pivasoftware.com>
 */

#include "cwmp_event.h"
#include "common.h"
#include "session.h"

pthread_mutex_t add_event_mutex = PTHREAD_MUTEX_INITIALIZER;

struct event_container *__cwmp_add_event_container(int event_code, char *command_key)
{
	struct event_container *event_container = NULL;
	list_for_each_entry(event_container, &cwmp_main->session->events, list) {
		if (event_container->code == event_code) {
			return event_container;
		}
		if (event_container->code > event_code) {
			break;
		}
	}
	event_container = calloc(1, sizeof(struct event_container));
	if (event_container == NULL) {
		return NULL;
	}
	INIT_LIST_HEAD(&(event_container->head_dm_parameter));
	list_add_tail(&(event_container->list), &(cwmp_main->session->events));
	event_container->code = event_code;
	event_container->command_key = command_key ? strdup(command_key) : strdup("");
	if ((cwmp_main->event_id < 0) || (cwmp_main->event_id >= MAX_INT_ID)) {
		cwmp_main->event_id = 0;
	}
	cwmp_main->event_id++;
	event_container->id = cwmp_main->event_id;
	return event_container;
}

struct event_container *cwmp_add_event_container(int event_code, char *command_key)
{
	pthread_mutex_lock(&add_event_mutex);
	struct event_container *event = __cwmp_add_event_container(event_code, command_key);
	pthread_mutex_unlock(&add_event_mutex);
	return event;
}
