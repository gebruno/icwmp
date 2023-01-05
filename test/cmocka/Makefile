
LIB_LDFLAGS:= -lmxml -luci -lblobmsg_json -lubox\
	      -ljson-c -lubus -lpthread -lcurl\
	      -lcrypto -luuid 
	      
LIB_CFLAGS:= -fPIC -I../../ -DLOPENSSL -g -O0
UNIT_TESTS:= icwmp_unit_testd

VALGRIND = /usr/bin/valgrind --xml=yes --xml-file=memory-report.xml --leak-check=full --show-reachable=yes --show-leak-kinds=all --errors-for-leak-kinds=all

OBJS_SOURCES := $(filter-out ../../src/datamodel.c,$(wildcard ../../src/*.c))
ICWMP_OBJS=$(patsubst ../../src/%.c, %.o, $(OBJS_SOURCES))
ICWMP_OBJS := $(filter-out datamodel.o, $(ICWMP_OBJS))
TEST_SRCS =  $(wildcard *.c)
TEST_OBJS = $(TEST_SRCS:=.o)

%.c.o: %.c
	$(CC) -g -O0 -I../../src/ -c -o $@ $<

%.o: ../../src/%.c
	$(CC) ${LIB_CFLAGS} -c -o $@ $<

libunit: ${ICWMP_OBJS}
	$(CC) ${LIB_CFLAGS} -shared -o libicwmp.so ${ICWMP_OBJS} ${LIB_LDFLAGS}
	
libcwmpdm: datamodel.o
	$(CC) ${LIB_CFLAGS} -shared -o libcwmpdm.so datamodel.o ${LIB_LDFLAGS} -lbbfdm
	
icwmp_unit_testd:
	make ${TEST_OBJS}
	$(CC) -o $@ ${TEST_OBJS} ${LIB_LDFLAGS} -lcmocka -L. -licwmp -lcwmpdm -Wl,-rpath=.
	
all: libunit ${UNIT_TESTS}
	$(VALGRIND) ./${UNIT_TESTS}
	mv memory-report.xml ../../
	echo "All test done"

clean:
	rm -fv *.o libicwmp.so libcwmpdm.so ${UNIT_TESTS}

.PHONY: clean unit-test