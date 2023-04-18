
LIB_LDFLAGS:= -lmxml -luci -lblobmsg_json -lubox\
	      -ljson-c -lubus -lpthread -lcurl\
	      -lcrypto -luuid 
	      
LIB_CFLAGS:= -fPIC -I../../ -DLOPENSSL -g -O0
UNIT_TESTS:= icwmp_unit_testd

VALGRIND = /usr/bin/valgrind --xml=yes --xml-file=/tmp/memory-report.xml --leak-check=full --show-reachable=yes --show-leak-kinds=all --errors-for-leak-kinds=all

ICWMP_OBJS=$(patsubst ../../src/%.c, %.o, $(wildcard ../../src/*.c))
TEST_SRCS =  $(wildcard *.c)
TEST_OBJS = $(TEST_SRCS:=.o)

%.c.o: %.c
	$(CC) -g -O0 -I../../src/ -c -o $@ $<

%.o: ../../src/%.c
	$(CC) ${LIB_CFLAGS} -c -o $@ $<

libunit: ${ICWMP_OBJS}
	$(CC) ${LIB_CFLAGS} -shared -o libicwmp.so ${ICWMP_OBJS} ${LIB_LDFLAGS}

icwmp_unit_testd:
	make ${TEST_OBJS}
	$(CC) -o $@ ${TEST_OBJS} ${LIB_LDFLAGS} -lcmocka -L. -licwmp -Wl,-rpath=.
	
all: libunit ${UNIT_TESTS}
	$(VALGRIND) ./${UNIT_TESTS}
	echo "All test done"

clean:
	rm -rf *.o libicwmp.so ${UNIT_TESTS} /tmp/memory-report.xml

.PHONY: clean unit-test
