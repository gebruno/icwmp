/*
 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	Copyright (C) 2013-2021 iopsys Software Solutions AB
 *	  Author Omar Kallel <omar.kallel@pivasoftware.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "common.h"
#include "uci_utils.h"

#define UCI_WRONG_PATH "cwmp.wrong_section.wrong_option"
#define UCI_VAR_WRONG_PATH "icwmp.wrong_section.wrong_option"

static int cwmp_uci_unit_tests_init(void **state)
{
	return 0;
}

static int cwmp_uci_unit_tests_clean(void **state)
{
	icwmp_cleanmem();
	return 0;
}

static void cwmp_uci_get_tests(void **state)
{

	char value[BUF_SIZE_256] = {0};
	int error;

	error = get_uci_path_value(NULL, "cwmp.acs.userid", value, BUF_SIZE_256);
	assert_int_equal(error, UCI_OK);
	assert_string_equal(value, "iopsys");

	value[0] = '\0';
	error = get_uci_path_value(NULL, UCI_WRONG_PATH, value, BUF_SIZE_256);
	assert_int_equal(error, UCI_ERR_NOTFOUND);
	assert_string_equal(value, "");

	value[0] = '\0';
	error = get_uci_path_value(VARSTATE_CONFIG, "icwmp.acs.dhcp_url", value, BUF_SIZE_256);
	assert_int_equal(error, UCI_OK);
	assert_string_equal(value, "http://192.168.103.160:8080/openacs/acs");

	value[0] = '\0';
	error = get_uci_path_value(VARSTATE_CONFIG, UCI_VAR_WRONG_PATH, value, BUF_SIZE_256);
	assert_int_equal(error, UCI_ERR_NOTFOUND);
	assert_string_equal(value, "");
}

static void cwmp_uci_add_tests(void **state)
{
	int error = UCI_OK;
	char value[BUF_SIZE_256] = {0};

	error = set_uci_path_value(NULL, "cwmp.new_acs", "acs");
	assert_int_equal(error, UCI_OK);

	error = set_uci_path_value(NULL, "cwmp.new_acs.test", "abc");
	assert_int_equal(error, UCI_OK);

	error = get_uci_path_value(NULL, "cwmp.new_acs.test", value, BUF_SIZE_256);
	assert_int_equal(error, UCI_OK);
	assert_string_equal(value, "abc");

	error = set_uci_path_value(NULL, "cwmp.new_acs.test", "");
	assert_int_equal(error, UCI_OK);

	memset(value, 0, BUF_SIZE_256);
	error = get_uci_path_value(NULL, "cwmp.new_acs.test", value, BUF_SIZE_256);
	assert_int_equal(error, UCI_ERR_NOTFOUND);
	assert_string_equal(value, "");

	error = set_uci_path_value(NULL, "cwmp.new_acs", "acs");
	assert_int_equal(error, UCI_OK);
}

int icwmp_uci_test(void)
{
	const struct CMUnitTest tests[] = {
		    cmocka_unit_test(cwmp_uci_get_tests),
		    cmocka_unit_test(cwmp_uci_add_tests)
	};

	return cmocka_run_group_tests(tests, cwmp_uci_unit_tests_init, cwmp_uci_unit_tests_clean);
}
