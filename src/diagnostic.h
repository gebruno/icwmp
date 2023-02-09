/*
 * diagnostic.h - Manage Diagnostics parameters from icwmp
 *
 * Copyright (C) 2021-2022, IOPSYS Software Solutions AB.
 *
 *	  Author Imen Bhiri <imen.bhiri@pivasoftware.com>
 *	  Author: Amin Ben Ramdhane <amin.benramdhane@pivasoftware.com>
 *	  Author: Omar Kallel <omar.kallel@pivasoftware.com>
 *
 * See LICENSE file for license related information.
 *
 */

#ifndef __DIAGNOSTIC__H
#define __DIAGNOSTIC__H

bool set_diagnostic_parameter_structure_value(char *parameter_name, char *value);

int cwmp_wifi_neighboring__diagnostics();
int cwmp_download_diagnostics();
int cwmp_upload_diagnostics();
int cwmp_ip_ping_diagnostics();
int cwmp_nslookup_diagnostics();
int cwmp_traceroute_diagnostics();
int cwmp_udp_echo_diagnostics();
int cwmp_serverselection_diagnostics();
int cwmp_ip_layer_capacity_diagnostics();

#endif
