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
#include "backupSession.h"
#include "log.h"

static struct event_container *__cwmp_add_event_container(int event_code, const char *command_key)
{
	struct event_container *event_container = NULL;
	struct list_head *ilist;

	list_for_each (ilist, &(cwmp_ctx.session->events)) {
		event_container = list_entry(ilist, struct event_container, list);
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
	list_add(&(event_container->list), ilist->prev);
	event_container->code = event_code;
	event_container->command_key = command_key ? strdup(command_key) : strdup("");
	event_container->next_session = true;
	if ((cwmp_ctx.event_id < 0) || (cwmp_ctx.event_id >= MAX_INT_ID)) {
		cwmp_ctx.event_id = 0;
	}
	cwmp_ctx.event_id++;
	event_container->id = cwmp_ctx.event_id;

	return event_container;
}

struct event_container *cwmp_add_event_container(int event_code, const char *command_key)
{
	struct event_container *event = __cwmp_add_event_container(event_code, command_key);
	return event;
}

void move_next_session_events_to_actual_session()
{
	struct event_container *event_container;

	struct list_head *event_container_list = &(cwmp_ctx.session->events);
	list_for_each_entry (event_container, event_container_list, list) {
		// cppcheck-suppress uninitvar
		if (cwmp_ctx.session->session_status.is_heartbeat && event_container->code != EVENT_IDX_14HEARTBEAT)
			continue;
		if ((!cwmp_ctx.session->session_status.is_heartbeat) && (event_container->code == EVENT_IDX_14HEARTBEAT))
			continue;
		event_container->next_session = false;
	}
}

int cwmp_remove_all_session_events()
{
	CWMP_LOG(DEBUG, "%s:%d entry", __func__, __LINE__);
	struct event_container *event_container = NULL, *node = NULL;
	struct list_head *events_container_list = &(cwmp_ctx.session->events);

	list_for_each_entry_safe(event_container, node, events_container_list, list) {
		if (event_container->code == EVENT_IDX_14HEARTBEAT || event_container->next_session) {
			continue;
		}

		bkp_session_delete_element("cwmp_event", event_container->id);
		FREE(event_container->command_key);
		cwmp_free_all_dm_parameter_list(&(event_container->head_dm_parameter));
		list_del(&(event_container->list));
		free(event_container);
	}

	bkp_session_save();
	CWMP_LOG(DEBUG, "%s:%d exit", __func__, __LINE__);
	return CWMP_OK;
}

int remove_single_event(int event_code)
{
	CWMP_LOG(DEBUG, "%s:%d entry", __func__, __LINE__);
	struct event_container *event_container = NULL, *node = NULL;
	struct list_head *events_container_list = &(cwmp_ctx.session->events);

	list_for_each_entry_safe(event_container, node, events_container_list, list) {
		if (event_container->code == event_code || !event_container->next_session) {
			bkp_session_delete_element("cwmp_event", event_container->id);
			FREE(event_container->command_key);
			cwmp_free_all_dm_parameter_list(&(event_container->head_dm_parameter));
			list_del(&(event_container->list));
			free(event_container);
		}
	}

	bkp_session_save();

	CWMP_LOG(DEBUG, "%s:%d exit", __func__, __LINE__);
	return CWMP_OK;
}
