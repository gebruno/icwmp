/*
 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	Copyright (C) 2013-2021 iopsys Software Solutions AB
 *	  Author Omar Kallel <omar.kallel@pivasoftware.com>
 */

#ifndef CWMP_EVENT_H
#define CWMP_EVENT_H

#include "event.h"

struct event_container *cwmp_add_event_container(int event_code, char *command_key);
void move_next_session_events_to_actual_session();
int cwmp_remove_all_session_events();
int remove_single_event(int event_code);

#endif //CWMP_EVENT_H
