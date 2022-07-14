#ifndef __ICWMP_UBUS_UTILS_H__
#define __ICWMP_UBUS_UTILS_H__

#include <libubus.h>
#include "common.h"

typedef void (*icwmp_ubus_cb)(struct ubus_request *req, int type, struct blob_attr *msg);

void bb_add_string(struct blob_buf *bb, const char *name, const char *value);
int icwmp_register_object(struct ubus_context *ctx);
int icwmp_delete_object(struct ubus_context *ctx);
int icwmp_ubus_invoke(const char *obj, const char *method, struct blob_attr *msg,
		      icwmp_ubus_cb icwmp_callback, void *callback_arg);

#endif /* __ICWMP_UBUS_UTILS_H__ */
