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
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <dirent.h>

#include "common.h"
#include "cwmp_uci.h"

#define UCI_WRONG_PATH "cwmp.wrong_section.wrong_option"
#define UCI_VAR_WRONG_PATH "icwmp.wrong_section.wrong_option"
static struct uci_list *list = NULL;

static int cwmp_uci_unit_tests_init(void **state)
{
	cwmp_uci_init();
	return 0;
}

static int cwmp_uci_unit_tests_clean(void **state)
{
	icwmp_cleanmem();
	cwmp_uci_exit();
	if (list != NULL)
		cwmp_free_uci_list(list);
	return 0;
}

static void cwmp_uci_get_tests(void **state)
{

	char *value = NULL;
	int error;

	error = uci_get_value("cwmp.acs.userid", &value);
	assert_int_equal(error, UCI_OK);
	assert_string_equal(value, "iopsys");

	error = uci_get_value(UCI_WRONG_PATH, &value);
	assert_int_equal(error, UCI_ERR_NOTFOUND);
	assert_null(value);

	error = uci_get_state_value("icwmp.acs.dhcp_url", &value);
	assert_int_equal(error, UCI_OK);
	assert_string_equal(value, "http://192.168.103.160:8080/openacs/acs");

	error = uci_get_state_value(UCI_VAR_WRONG_PATH, &value);
	assert_int_equal(error, UCI_ERR_NOTFOUND);
	assert_null(value);
}

static void cwmp_uci_add_tests(void **state)
{
	struct uci_section *s = NULL;
	int error = UCI_OK;

	error = cwmp_uci_add_section("cwmp", "acs", UCI_STANDARD_CONFIG, &s);
	assert_non_null(s);
	assert_int_equal(error, UCI_OK);
	cwmp_commit_package("cwmp", UCI_STANDARD_CONFIG);
	assert_int_equal(strncmp(section_name(s), "cfg", 3), 0);
	s = NULL;

	error = cwmp_uci_add_section("new_package", "new_section", UCI_STANDARD_CONFIG, &s);
	assert_non_null(s);
	assert_int_equal(error, UCI_OK);
	cwmp_commit_package("new_package", UCI_STANDARD_CONFIG);
	assert_int_equal(strncmp(section_name(s), "cfg", 3), 0);
	s = NULL;

	error = cwmp_uci_add_section_with_specific_name("cwmp", "acs", "new_acs", UCI_STANDARD_CONFIG);
	assert_int_equal(error, UCI_OK);
	cwmp_commit_package("cwmp", UCI_STANDARD_CONFIG);

	error = cwmp_uci_add_section_with_specific_name("cwmp", "acs", "new_acs", UCI_STANDARD_CONFIG);
	assert_int_equal(error, UCI_ERR_DUPLICATE);
	cwmp_commit_package("cwmp", UCI_STANDARD_CONFIG);

}

static void cwmp_uci_list_tests(void **state)
{
	int error = UCI_OK;

	error = cwmp_uci_add_list_value("cwmp", "cpe", "optionlist", "val1", UCI_STANDARD_CONFIG);
	assert_int_equal(error, UCI_OK);
	error = cwmp_uci_add_list_value("cwmp", "cpe", "optionlist", "val2", UCI_STANDARD_CONFIG);
	assert_int_equal(error, UCI_OK);
	cwmp_commit_package("cwmp", UCI_STANDARD_CONFIG);

	error = cwmp_uci_add_list_value("cwmp", "wrong_section", "optionlist", "val1", UCI_STANDARD_CONFIG);
	assert_int_equal(error, UCI_ERR_INVAL);
	error = cwmp_uci_add_list_value("cwmp", "wrong_section", "optionlist", "val2", UCI_STANDARD_CONFIG);
	assert_int_equal(error, UCI_ERR_INVAL);
	cwmp_commit_package("cwmp", UCI_STANDARD_CONFIG);
}

int icwmp_uci_test(void)
{
	const struct CMUnitTest tests[] = {
		    cmocka_unit_test(cwmp_uci_get_tests),
			cmocka_unit_test(cwmp_uci_add_tests),
			cmocka_unit_test(cwmp_uci_list_tests)
	};

	return cmocka_run_group_tests(tests, cwmp_uci_unit_tests_init, cwmp_uci_unit_tests_clean);
}
