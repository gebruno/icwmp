MAINTAINERCLEANFILES = Makefile.in

CWMP_VERSION = 3.0.1

bin_PROGRAMS = icwmpd

icwmpd_SOURCES =			\
	./src/common.c				\
	./src/log.c				\
	./src/cwmp_uci.c			\
	./src/config.c				\
	./src/session.c			\
	./src/backupSession.c		\
	./src/digauth.c			\
	./src/event.c				\
	./src/http.c				\
	./src/ubus_utils.c				\
	./src/datamodel_interface.c \
	./src/cwmp_cli.c			\
	./src/notifications.c		\
	./src/cwmp_zlib.c			\
	./src/cwmp_du_state.c		\
	./src/download.c			\
	./src/upload.c				\
	./src/sched_inform.c		\
	./src/xml.c				\
	./src/rpc.c			\
	./src/diagnostic.c			\
	./src/reboot.c 			\
	./src/heartbeat.c		\
	./src/cwmp.c			\
	./src/ssl_utils.c

icwmpd_CFLAGS =			\
	$(AM_CFLAGS)		\
	$(LIBUCI_CFLAGS)	\
	$(LIBUBOX_CFLAGS)	\
	$(LIBUBUS_CFLAGS)	\
	$(LIBCURL_CFLAGS) 	\
	-Wall -Wextra -Werror   \
	-Wformat

icwmpd_LDFLAGS =		\
	$(AM_LDFLAGS)		\
	$(LIBUCI_LDFLAGS)	\
	$(LIBUBOX_LDFLAGS)	\
	$(LIBUBUS_LDFLAGS)	\
	$(LIBCURL_LDFLAGS)	\
	-lmxml

icwmpd_LDADD =			\
	$(AM_LIBS)			\
	$(LIBUCI_LIBS)		\
	$(LIBUBOX_LIBS)		\
	$(LIBUBUS_LIBS)		\
	$(LIBCURL_LIBS)		\
	$(LIBPTHREAD_LIBS)	\
	$(LCRYPTO_LIBS)		\
	$(LSSL_LIBS)		\
	$(LIBJSON_LIBS)		\
	$(LBLOBMSG_LIBS)	\
	$(LIBZ_LIBS)		\
	$(LIBM_LIBS)		

icwmpd_CFLAGS+=-DCWMP_VERSION=\"$(CWMP_VERSION)\"
icwmpd_LDFLAGS+=-DCWMP_VERSION=\"$(CWMP_VERSION)\"

icwmpd_CFLAGS+=-I..
