/*
 * config.h - load/store icwmp application configuration
 *
 * Copyright (C) 2021-2022, IOPSYS Software Solutions AB.
 *
 *	  Author Omar Kallel <omar.kallel@pivasoftware.com>
 *	  Author Anis Ellouze <anis.ellouze@pivasoftware.com>
 *
 * See LICENSE file for license related information.
 *
 */

#ifndef _CONFIG_H__
#define _CONFIG_H__

#include "cwmp_uci.h"

extern pthread_mutex_t mutex_config_load;

int global_conf_init();
int get_global_config();
int cwmp_get_deviceid();
int cwmp_config_reload();
int get_preinit_config();
void cwmp_config_load();
#endif
