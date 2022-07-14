/*
 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	Copyright (C) 2013-2021 iopsys Software Solutions AB
 *	  Author Mohamed Kallel <mohamed.kallel@pivasoftware.com>
 *	  Author Ahmed Zribi <ahmed.zribi@pivasoftware.com>
 *	  Author Omar Kallel <omar.kallel@pivasoftware.com>
 */

#include "xml.h"
#include "log.h"
#include "notifications.h"
#include "http.h"
#include "cwmp_zlib.h"

static const char *soap_env_url = "http://schemas.xmlsoap.org/soap/envelope/";
static const char *soap_enc_url = "http://schemas.xmlsoap.org/soap/encoding/";
static const char *xsd_url = "http://www.w3.org/2001/XMLSchema";
static const char *xsi_url = "http://www.w3.org/2001/XMLSchema-instance";

const char *cwmp_urls[] = { "urn:dslforum-org:cwmp-1-0", "urn:dslforum-org:cwmp-1-1", "urn:dslforum-org:cwmp-1-2", "urn:dslforum-org:cwmp-1-2", "urn:dslforum-org:cwmp-1-2", "urn:dslforum-org:cwmp-1-2", NULL };

mxml_node_t * /* O - Element node or NULL */
mxmlFindElementOpaque(mxml_node_t *node, /* I - Current node */
		      mxml_node_t *top, /* I - Top node */
		      const char *text, /* I - Element text, if NULL return NULL */
		      int descend) /* I - Descend into tree - MXML_DESCEND, MXML_NO_DESCEND, or MXML_DESCEND_FIRST */
{
	if (!node || !top || !text)
		return (NULL);

	node = mxmlWalkNext(node, top, descend);

	while (node != NULL) {
		const char *op = mxmlGetOpaque(node);
		if (mxmlGetType(node) == MXML_OPAQUE && op && (!strcmp(op, text))) {
			return (node);
		}

		if (descend == MXML_DESCEND)
			node = mxmlWalkNext(node, top, MXML_DESCEND);
		else
			node = mxmlGetNextSibling(node);
	}
	return (NULL);
}

char *xml__get_attribute_name_by_value(mxml_node_t *node,	const char  *value)
{
	int attributes_nbre = mxmlElementGetAttrCount(node);
	int i;
	for (i = 0; i < attributes_nbre; i++) {
		char *attr_name = NULL;
		const char *attr_value = mxmlElementGetAttrByIndex(node, i, (const char **)&attr_name);
		if (strcmp(attr_value, value) == 0)
			return attr_name;
	}
	return NULL;
}

int xml_recreate_namespace(mxml_node_t *tree)
{
	const char *cwmp_urn;
	int i;
	mxml_node_t *b = tree;

	do {
		char *c;
		FREE(ns.soap_env);
		FREE(ns.soap_enc);
		FREE(ns.xsd);
		FREE(ns.xsi);
		FREE(ns.cwmp);

		c = (char *)xml__get_attribute_name_by_value(b, soap_env_url);
		if (c && *(c + 5) == ':') {
			ns.soap_env = strdup((c + 6));
		} else {
			continue;
		}

		c = (char *)xml__get_attribute_name_by_value(b, soap_enc_url);
		if (c && *(c + 5) == ':') {
			ns.soap_enc = strdup((c + 6));
		} else {
			continue;
		}

		c = (char *)xml__get_attribute_name_by_value(b, xsd_url);
		if (c && *(c + 5) == ':') {
			ns.xsd = strdup((c + 6));
		} else {
			continue;
		}

		c = (char *)xml__get_attribute_name_by_value(b, xsi_url);
		if (c && *(c + 5) == ':') {
			ns.xsi = strdup((c + 6));
		} else {
			continue;
		}

		for (i = 0; cwmp_urls[i] != NULL; i++) {
			cwmp_urn = cwmp_urls[i];
			c = (char *)xml__get_attribute_name_by_value(b, cwmp_urn);
			if (c && *(c + 5) == ':') {
				ns.cwmp = strdup((c + 6));
				break;
			}
		}

		if (!ns.cwmp)
			continue;

		return 0;
	} while ((b = mxmlWalkNext(b, tree, MXML_DESCEND)));

	return -1;
}

void xml_exit(void)
{
	FREE(ns.soap_env);
	FREE(ns.soap_enc);
	FREE(ns.xsd);
	FREE(ns.xsi);
	FREE(ns.cwmp);
}

int xml_send_message(struct cwmp *cwmp, struct session *session, struct rpc *rpc)
{
	char *s, *msg_out = NULL, *msg_in = NULL;
	char c[512];
	int msg_out_len = 0, f, r = 0;
	mxml_node_t *b;

	if (session->tree_out) {
		unsigned char *zmsg_out;
		msg_out = mxmlSaveAllocString(session->tree_out, whitespace_cb);
		CWMP_LOG_XML_MSG(DEBUG, msg_out, XML_MSG_OUT);
		if (cwmp->conf.compression != COMP_NONE) {
			if (zlib_compress(msg_out, &zmsg_out, &msg_out_len, cwmp->conf.compression)) {
				return -1;
			}
			FREE(msg_out);
			msg_out = (char *)zmsg_out;
		} else {
			msg_out_len = strlen(msg_out);
		}
	}
	while (1) {
		f = 0;
		if (http_send_message(cwmp, msg_out, msg_out_len, &msg_in)) {
			goto error;
		}
		if (msg_in) {
			CWMP_LOG_XML_MSG(DEBUG, msg_in, XML_MSG_IN);
			if ((s = strstr(msg_in, "<FaultCode>")))
				sscanf(s, "<FaultCode>%d</FaultCode>", &f);
			if (f) {
				if (f == 8005) {
					r++;
					if (r < 5) {
						FREE(msg_in);
						continue;
					}
					goto error;
				} else if (rpc && rpc->type != RPC_ACS_INFORM) {
					break;
				} else {
					goto error;
				}
			} else {
				break;
			}
		} else {
			goto end;
		}
	}

	session->tree_in = mxmlLoadString(NULL, msg_in, MXML_OPAQUE_CALLBACK);
	if (!session->tree_in)
		goto error;
	xml_recreate_namespace(session->tree_in);
	/* get NoMoreRequests or HolRequest*/
	session->hold_request = false;

	if (snprintf(c, sizeof(c), "%s:%s", ns.cwmp, "NoMoreRequests") == -1)
		goto error;
	b = mxmlFindElement(session->tree_in, session->tree_in, c, NULL, NULL, MXML_DESCEND);
	if (b) {
		b = mxmlWalkNext(b, session->tree_in, MXML_DESCEND_FIRST);
		if (b && mxmlGetType(b) == MXML_OPAQUE && mxmlGetOpaque(b))
			session->hold_request = atoi(mxmlGetOpaque(b));
	} else {
		if (snprintf(c, sizeof(c), "%s:%s", ns.cwmp, "HoldRequests") == -1)
			goto error;

		b = mxmlFindElement(session->tree_in, session->tree_in, c, NULL, NULL, MXML_DESCEND);
		if (b) {
			b = mxmlWalkNext(b, session->tree_in, MXML_DESCEND_FIRST);
			if (b && mxmlGetType(b) == MXML_OPAQUE && mxmlGetOpaque(b))
				session->hold_request = atoi(mxmlGetOpaque(b));
		}
	}

end:
	FREE(msg_out);
	FREE(msg_in);
	return 0;

error:
	FREE(msg_out);
	FREE(msg_in);
	return -1;
}

int xml_prepare_msg_out(struct session *session)
{
	struct cwmp *cwmp = &cwmp_main;
	struct config *conf;
	conf = &(cwmp->conf);
	mxml_node_t *n;

	load_response_xml_schema(&session->tree_out);
	if (!session->tree_out)
		return -1;

	n = mxmlFindElement(session->tree_out, session->tree_out, "soap_env:Envelope", NULL, NULL, MXML_DESCEND);
	if (!n) {
		return -1;
	}

	mxmlElementSetAttr(n, "xmlns:cwmp", cwmp_urls[(conf->amd_version) - 1]);
	if (!session->tree_out)
		return -1;

	return 0;
}

int xml_set_cwmp_id(struct session *session)
{
	char c[32];
	mxml_node_t *b;

	/* define cwmp id */
	if (snprintf(c, sizeof(c), "%u", ++(cwmp_main.cwmp_id)) == -1)
		return -1;

	b = mxmlFindElement(session->tree_out, session->tree_out, "cwmp:ID", NULL, NULL, MXML_DESCEND);
	if (!b)
		return -1;

	b = mxmlNewOpaque(b, c);
	if (!b)
		return -1;

	return 0;
}

int xml_set_cwmp_id_rpc_cpe(struct session *session)
{
	char c[512];
	mxml_node_t *b;

	/* handle cwmp:ID */
	if (snprintf(c, sizeof(c), "%s:%s", ns.cwmp, "ID") == -1)
		return -1;

	b = mxmlFindElement(session->tree_in, session->tree_in, c, NULL, NULL, MXML_DESCEND);

	if (b) {
		/* ACS send ID parameter */
		b = mxmlWalkNext(b, session->tree_in, MXML_DESCEND_FIRST);
		if (!b || mxmlGetType(b) != MXML_OPAQUE || !mxmlGetOpaque(b))
			return 0;
		snprintf(c, sizeof(c), "%s", mxmlGetOpaque(b));

		b = mxmlFindElement(session->tree_out, session->tree_out, "cwmp:ID", NULL, NULL, MXML_DESCEND);
		if (!b)
			return -1;

		b = mxmlNewOpaque(b, c);
		if (!b)
			return -1;
	} else {
		/* ACS does not send ID parameter */
		int r = xml_set_cwmp_id(session);
		return r;
	}
	return 0;
}

const char *get_node_tab_space(mxml_node_t *node)
{
	static char tab_space[10 * sizeof(CWMP_MXML_TAB_SPACE) + 1];
	int count = 0;

	memset(tab_space, 0, sizeof(tab_space));
	while ((node = mxmlGetParent(node))) {
		count = count + 1;
	}

	if (count) {
		snprintf(tab_space, sizeof(tab_space), "%*s", (int)(count * sizeof(CWMP_MXML_TAB_SPACE)), "");
	}

	return tab_space;
}

const char *whitespace_cb(mxml_node_t *node, int where __attribute__((unused)))
{
	if (mxmlGetType(node) != MXML_ELEMENT)
		return NULL;

	switch (where) {
	case MXML_WS_BEFORE_CLOSE:
		if (mxmlGetFirstChild(node) && mxmlGetType(mxmlGetFirstChild(node)) != MXML_ELEMENT)
			return NULL;

		return get_node_tab_space(node);
	case MXML_WS_BEFORE_OPEN:
		if (where == MXML_WS_BEFORE_CLOSE && mxmlGetFirstChild(node) && mxmlGetType(mxmlGetFirstChild(node)) != MXML_ELEMENT)
			return NULL;

		return get_node_tab_space(node);
	case MXML_WS_AFTER_OPEN:
		return ((mxmlGetFirstChild(node) == NULL || mxmlGetType(mxmlGetFirstChild(node)) == MXML_ELEMENT) ? "\n" : NULL);
	case MXML_WS_AFTER_CLOSE:
		return "\n";
	default:
		return NULL;
	}

	return NULL;
}

char *xml_get_cwmp_version(int version)
{
	static char versions[60];
	unsigned pos = 0;
	int k;

	versions[0] = '\0';
	for (k = 0; k < version; k++) {
		pos += snprintf(&versions[pos], sizeof(versions) - pos, "1.%d, ", k);
	}

	if (pos)
		versions[pos - 2] = 0;

	return versions;
}

static int xml_prepare_lwnotifications(mxml_node_t *parameter_list)
{
	mxml_node_t *b, *n;

	struct list_head *p;
	struct cwmp_dm_parameter *lw_notification;
	list_for_each (p, &list_lw_value_change) {
		lw_notification = list_entry(p, struct cwmp_dm_parameter, list);

		n = mxmlNewElement(parameter_list, "Param");
		if (!n)
			goto error;

		b = mxmlNewElement(n, "Name");
		if (!b)
			goto error;

		b = mxmlNewOpaque(b, lw_notification->name);
		if (!b)
			goto error;

		b = mxmlNewElement(n, "Value");
		if (!b)
			goto error;
#ifdef ACS_MULTI
		mxmlElementSetAttr(b, "xsi:type", lw_notification->type);
#endif
		b = mxmlNewOpaque(b, lw_notification->value);
		if (!b)
			goto error;
	}
	return 0;

error:
	return -1;
}

int xml_prepare_lwnotification_message(char **msg_out)
{
	mxml_node_t *lw_tree;

	load_notification_xml_schema(&lw_tree);
	if (!lw_tree)
		goto error;

	*msg_out = mxmlSaveAllocString(lw_tree, whitespace_cb);

	mxmlDelete(lw_tree);
	return 0;

error:
	return -1;
}

void load_notification_xml_schema(mxml_node_t **tree)
{
	char declaration[1024] = {0};
	struct cwmp *cwmp = &cwmp_main;
	struct config *conf;
	conf = &(cwmp->conf);
	char *c = NULL;

	if (tree == NULL)
		return;

	*tree = NULL;

	snprintf(declaration, sizeof(declaration), "?xml version=\"1.0\" encoding=\"UTF-8\"?");
	mxml_node_t *xml = mxmlNewElement(NULL, declaration);
	if (xml == NULL)
		return;

	mxml_node_t *notification = mxmlNewElement(xml, "Notification");
	if (notification == NULL) {
		MXML_DELETE(xml);
		return;
	}

	mxmlElementSetAttr(notification, "xmlns", "urn:broadband-forum-org:cwmp:lwnotif-1-0");
	mxmlElementSetAttr(notification, "xmlns:xs", xsd_url);
	mxmlElementSetAttr(notification, "xmlns:xsi", xsi_url);
	mxmlElementSetAttr(notification, "xsi:schemaLocation", "urn:broadband-forum-org:cwmp:lxnotif-1-0 http://www.broadband-forum.org/cwmp/cwmp-UDPLightweightNotification-1-0.xsd");

	mxml_node_t *ts = mxmlNewElement(notification, "TS");
	if (ts == NULL) {
		MXML_DELETE(xml);
		return;
	}

	if (cwmp_asprintf(&c, "%ld", time(NULL)) == -1) {
		MXML_DELETE(xml);
		return;
	}

	if (NULL == mxmlNewOpaque(ts, c)) {
		FREE(c);
		MXML_DELETE(xml);
		return;
	}

	FREE(c);

	mxml_node_t *un = mxmlNewElement(notification, "UN");
	if (un == NULL) {
		MXML_DELETE(xml);
		return;
	}

	if (NULL == mxmlNewOpaque(un, conf->acs_userid)) {
		MXML_DELETE(xml);
		return;
	}

	mxml_node_t *cn = mxmlNewElement(notification, "CN");
	if (cn == NULL) {
		MXML_DELETE(xml);
		return;
	}

	c = (char *)calculate_lwnotification_cnonce();
	if (!c) {
		MXML_DELETE(xml);
		return;
	}

	if (NULL == mxmlNewOpaque(cn, c)) {
		FREE(c);
		MXML_DELETE(xml);
		return;
	}

	FREE(c);

	mxml_node_t *oui = mxmlNewElement(notification, "OUI");
	if (oui == NULL) {
		MXML_DELETE(xml);
		return;
	}

	if (NULL == mxmlNewOpaque(oui, cwmp->deviceid.oui)) {
		MXML_DELETE(xml);
		return;
	}

	mxml_node_t *pclass = mxmlNewElement(notification, "ProductClass");
	if (pclass == NULL) {
		MXML_DELETE(xml);
		return;
	}

	if (NULL == mxmlNewOpaque(pclass, cwmp->deviceid.productclass ? cwmp->deviceid.productclass : "")) {
		MXML_DELETE(xml);
		return;
	}

	mxml_node_t *slno = mxmlNewElement(notification, "SerialNumber");
	if (slno == NULL) {
		MXML_DELETE(xml);
		return;
	}

	if (NULL == mxmlNewOpaque(slno, cwmp->deviceid.serialnumber ? cwmp->deviceid.serialnumber : "")) {
		MXML_DELETE(xml);
		return;
	}

	if (xml_prepare_lwnotifications(notification)) {
		MXML_DELETE(xml);
		return;
	}

	*tree = xml;
}

void load_response_xml_schema(mxml_node_t **schema)
{
	char declaration[1024] = {0};

	if (schema == NULL)
		return;

	*schema = NULL;

	snprintf(declaration, sizeof(declaration), "?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?");
	mxml_node_t *xml = mxmlNewElement(NULL, declaration);
	if (xml == NULL)
		return;

	mxml_node_t *envlp = mxmlNewElement(xml, "soap_env:Envelope");
	if (envlp == NULL) {
		MXML_DELETE(xml);
		return;
	}

	mxmlElementSetAttr(envlp, "xmlns:soap_env", soap_env_url);
	mxmlElementSetAttr(envlp, "xmlns:soap_enc", soap_enc_url);
	mxmlElementSetAttr(envlp, "xmlns:xsd", xsd_url);
	mxmlElementSetAttr(envlp, "xmlns:xsi", xsi_url);

	mxml_node_t *header = mxmlNewElement(envlp, "soap_env:Header");
	if (header == NULL) {
		MXML_DELETE(xml);
		return;
	}

	mxml_node_t *id = mxmlNewElement(header, "cwmp:ID");
	if (id == NULL) {
		MXML_DELETE(xml);
		return;
	}

	mxmlElementSetAttr(id, "soap_env:mustUnderstand", "1");

	if (NULL == mxmlNewElement(envlp, "soap_env:Body")) {
		MXML_DELETE(xml);
		return;
	}

	*schema = xml;
}
