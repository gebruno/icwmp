/*
 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	Copyright (C) 2013-2021 iopsys Software Solutions AB
 *	  Author Omar Kallel <omar.kallel@pivasoftware.com>
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <dirent.h>
#include <mxml.h>

#include "common.h"
#include "backupSession.h"
#include "xml.h"
#include "config.h"
#include "event.h"

static struct cwmp cwmp_main_test = { 0 };

static int bkp_session_unit_tests_init(void **state)
{
	cwmp_main = (struct cwmp*)calloc(1, sizeof(struct cwmp));
	create_cwmp_session_structure();
	return 0;
}

static int bkp_session_unit_tests_clean(void **state)
{
	icwmp_cleanmem();
	clean_cwmp_session_structure();
	FREE(cwmp_main);
	return 0;
}

static void cwmp_backup_session_unit_test(void **state)
{
	remove(CWMP_BKP_FILE);
	mxml_node_t *backup_tree = NULL, *n = NULL;

	/*
	 * Init backup session
	 */
	int error = cwmp_init_backup_session(NULL, ALL);
	assert_int_equal(error, 0);
	bkp_session_save();
	FILE *pFile;
	pFile = fopen(CWMP_BKP_FILE, "r");
	backup_tree = mxmlLoadFile(NULL, pFile, MXML_OPAQUE_CALLBACK);
	fclose(pFile);
	assert_non_null(backup_tree);
	assert_string_equal(mxmlGetElement(backup_tree), "cwmp");
	assert_null(mxmlGetFirstChild(backup_tree));
	MXML_DELETE(backup_tree);

	/*
	 * Insert Event
	 */
	mxml_node_t *bkp_event1 = NULL, *bkp_event2 = NULL, *event_tree1 = NULL, *event_tree2;

	// Case of one event
	bkp_event1 = bkp_session_insert_event(EVENT_IDX_4VALUE_CHANGE, "4 VALUE CHANGE", 0);
	bkp_session_save();
	pFile = fopen(CWMP_BKP_FILE, "r");
	backup_tree = mxmlLoadFile(NULL, pFile, MXML_OPAQUE_CALLBACK);
	fclose(pFile);
	assert_non_null(bkp_event1);
	event_tree1 = mxmlFindElement(backup_tree, backup_tree, "cwmp_event", NULL, NULL, MXML_DESCEND);
	assert_non_null(event_tree1);
	n = mxmlFindElement(event_tree1, event_tree1, "index", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	assert_int_equal(atoi(mxmlGetOpaque(mxmlGetFirstChild(n))), EVENT_IDX_4VALUE_CHANGE);
	n = mxmlFindElement(event_tree1, event_tree1, "id", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	assert_int_equal(atoi(mxmlGetOpaque(mxmlGetFirstChild(n))), 0);
	n = mxmlFindElement(event_tree1, event_tree1, "command_key", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	assert_string_equal(mxmlGetOpaque(mxmlGetFirstChild(n)), "4 VALUE CHANGE");
	MXML_DELETE(bkp_event1);
	bkp_session_save();
	MXML_DELETE(backup_tree);

	//case of two events with different ids
	bkp_event1 = bkp_session_insert_event(EVENT_IDX_1BOOT, "1 BOOT", 0);
	bkp_event2 = bkp_session_insert_event(EVENT_IDX_4VALUE_CHANGE, "4 VALUE CHANGE", 1);
	bkp_session_save();
	pFile = fopen(CWMP_BKP_FILE, "r");
	backup_tree = mxmlLoadFile(NULL, pFile, MXML_OPAQUE_CALLBACK);
	fclose(pFile);
	assert_non_null(bkp_event1);
	assert_non_null(bkp_event2);
	event_tree1 = mxmlFindElement(backup_tree, backup_tree, "cwmp_event", NULL, NULL, MXML_DESCEND);
	event_tree2 = mxmlFindElement(event_tree1, backup_tree, "cwmp_event", NULL, NULL, MXML_DESCEND);

	assert_non_null(event_tree1);
	n = mxmlFindElement(event_tree1, event_tree1, "index", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	assert_int_equal(atoi(mxmlGetOpaque(mxmlGetFirstChild(n))), EVENT_IDX_1BOOT);
	n = mxmlFindElement(event_tree1, event_tree1, "id", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	assert_int_equal(atoi(mxmlGetOpaque(mxmlGetFirstChild(n))), 0);
	n = mxmlFindElement(event_tree1, event_tree1, "command_key", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	assert_string_equal(mxmlGetOpaque(mxmlGetFirstChild(n)), "1 BOOT");

	assert_non_null(event_tree2);
	n = mxmlFindElement(event_tree2, event_tree2, "index", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	assert_int_equal(atoi(mxmlGetOpaque(mxmlGetFirstChild(n))), EVENT_IDX_4VALUE_CHANGE);
	n = mxmlFindElement(event_tree2, event_tree2, "id", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	assert_int_equal(atoi(mxmlGetOpaque(mxmlGetFirstChild(n))), 1);
	n = mxmlFindElement(event_tree2, event_tree2, "command_key", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	assert_string_equal(mxmlGetOpaque(mxmlGetFirstChild(n)), "4 VALUE CHANGE");

	MXML_DELETE(bkp_event1);
	MXML_DELETE(bkp_event2);
	bkp_session_save();
	MXML_DELETE(backup_tree);
	bkp_event1 = NULL;
	bkp_event2 = NULL;

	/*
	 * Insert Download
	 */
	struct download *download = NULL;
	download = icwmp_calloc(1, sizeof(struct download));
	download->command_key = icwmp_strdup("download_key");
	download->file_size = 0;
	download->file_type = icwmp_strdup("1 Firmware Upgrade Image");
	download->password = icwmp_strdup("iopsys");
	download->username = icwmp_strdup("iopsys");
	download->url = icwmp_strdup("http://192.168.1.160:8080/openacs/acs");
	bkp_session_insert_download(download);
	bkp_session_save();
	pFile = fopen(CWMP_BKP_FILE, "r");
	backup_tree = mxmlLoadFile(NULL, pFile, MXML_OPAQUE_CALLBACK);
	fclose(pFile);

	mxml_node_t *download_tree = NULL;
	download_tree = mxmlFindElement(backup_tree, backup_tree, "download", NULL, NULL, MXML_DESCEND);
	assert_non_null(download_tree);
	n = mxmlFindElement(download_tree, download_tree, "url", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	assert_string_equal(mxmlGetOpaque(mxmlGetFirstChild(n)), "http://192.168.1.160:8080/openacs/acs");
	n = mxmlFindElement(download_tree, download_tree, "command_key", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	assert_string_equal(mxmlGetOpaque(mxmlGetFirstChild(n)), "download_key");
	n = mxmlFindElement(download_tree, download_tree, "file_type", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	assert_string_equal(mxmlGetOpaque(mxmlGetFirstChild(n)), "1 Firmware Upgrade Image");
	n = mxmlFindElement(download_tree, download_tree, "username", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	assert_string_equal(mxmlGetOpaque(mxmlGetFirstChild(n)), "iopsys");
	n = mxmlFindElement(download_tree, download_tree, "password", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	assert_string_equal(mxmlGetOpaque(mxmlGetFirstChild(n)), "iopsys");
	n = mxmlFindElement(download_tree, download_tree, "file_size", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	assert_string_equal(mxmlGetOpaque(mxmlGetFirstChild(n)), "0");
	n = mxmlFindElement(download_tree, download_tree, "time", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	MXML_DELETE(backup_tree);

	/*
	 * Delete download
	 */
	bkp_session_delete_download(download);
	bkp_session_save();
	pFile = fopen(CWMP_BKP_FILE, "r");
	backup_tree = mxmlLoadFile(NULL, pFile, MXML_OPAQUE_CALLBACK);
	fclose(pFile);
	assert_non_null(backup_tree);
	assert_string_equal(mxmlGetElement(backup_tree), "cwmp");
	assert_null(mxmlGetFirstChild(backup_tree));
	MXML_DELETE(backup_tree);

	/*
	 * Insert TransferComplete bkp_session_delete_transfer_complete
	 */
	struct transfer_complete *p;
	p = icwmp_calloc(1, sizeof(struct transfer_complete));
	p->command_key = icwmp_strdup("transfer_complete_key");
	p->start_time = icwmp_strdup(get_time(time(NULL)));
	p->complete_time = icwmp_strdup(get_time(time(NULL)));
	p->old_software_version = icwmp_strdup("iopsys_img_old");
	p->type = TYPE_DOWNLOAD;
	p->fault_code = FAULT_CPE_NO_FAULT;
	bkp_session_insert_transfer_complete(p);
	bkp_session_save();
	pFile = fopen(CWMP_BKP_FILE, "r");
	backup_tree = mxmlLoadFile(NULL, pFile, MXML_OPAQUE_CALLBACK);
	fclose(pFile);

	mxml_node_t *transfer_complete_tree = NULL;
	transfer_complete_tree = mxmlFindElement(backup_tree, backup_tree, "transfer_complete", NULL, NULL, MXML_DESCEND);
	assert_non_null(transfer_complete_tree);
	n = mxmlFindElement(transfer_complete_tree, transfer_complete_tree, "command_key", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	assert_string_equal(mxmlGetOpaque(mxmlGetFirstChild(n)), "transfer_complete_key");
	n = mxmlFindElement(transfer_complete_tree, transfer_complete_tree, "start_time", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(transfer_complete_tree, transfer_complete_tree, "complete_time", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(transfer_complete_tree, transfer_complete_tree, "old_software_version", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	assert_string_equal(mxmlGetOpaque(mxmlGetFirstChild(n)), "iopsys_img_old");
	n = mxmlFindElement(transfer_complete_tree, transfer_complete_tree, "fault_code", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	assert_int_equal(atoi(mxmlGetOpaque(mxmlGetFirstChild(n))), FAULT_CPE_NO_FAULT);
	MXML_DELETE(backup_tree);

	/*
	 * Delete TransferComplete
	 */
	bkp_session_delete_transfer_complete(p);
	bkp_session_save();
	pFile = fopen(CWMP_BKP_FILE, "r");
	backup_tree = mxmlLoadFile(NULL, pFile, MXML_OPAQUE_CALLBACK);
	fclose(pFile);
	assert_non_null(backup_tree);
	assert_string_equal(mxmlGetElement(backup_tree), "cwmp");
	assert_null(mxmlGetFirstChild(backup_tree));
	MXML_DELETE(backup_tree);

	bkp_tree_clean();
}

int icwmp_backup_session_test(void)
{
	const struct CMUnitTest tests[] = { //
		    cmocka_unit_test(cwmp_backup_session_unit_test),
	};

	return cmocka_run_group_tests(tests, bkp_session_unit_tests_init, bkp_session_unit_tests_clean);
}
