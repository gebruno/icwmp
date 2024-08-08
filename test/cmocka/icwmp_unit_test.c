#include <stdio.h>
#include "icwmp_unit_test.h"
#include "ubus_utils.h"

struct ubus_context *ubus_ctx = NULL;

int main()
{
	int ret = 0;

	icwmp_connect_ubus();
	ret += icwmp_notifications_test();
	ret += icwmp_cli_unit_test();
	ret += icwmp_soap_msg_test();
	ret += icwmp_uci_test();
	ret += icwmp_datamodel_interface_test();
	ret += icwmp_backup_session_test();
	ret += icwmp_download_unit_test();
	icwmp_free_ubus();

	return ret;
}
