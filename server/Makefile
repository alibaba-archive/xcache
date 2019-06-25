
CCACHE:= ccacheXX

ifeq ($(CCACHE), $(wildcard $(CCACHE)))
CC:= $(CCACHE) gcc
else
CC:= gcc
endif
AR:= ar

CFLAGS:= -g -Wall -W -D_GNU_SOURCE -Wno-unused-function
LDFLAGS:= -lpthread

BIN:= xcache
SOURCES_BIN:= main.c hash.c slab.c page.c bklist.c config.c clock.c  block.c lru.c event.c net.c connect.c worker.c local.c stats.c daemon.c args.c backup.c
BIN_LDFLAGS := -Wl,-Bstatic -L. -lxcache -Wl,-Bdynamic -lpthread
OBJECTS_BIN:= $(SOURCES_BIN:.c=.o)

TEST:= test
SOURCES_TEST:= test.c config.c hash.c backup.c args.c daemon.c
OBJECTS_TEST:= $(SOURCES_TEST:.c=.o)
LDFLAGS_TEST:= $(BIN_LDFLAGS)


MONITOR:= xcache_monitor
SOURCES_MONITOR:= monitor.c
OBJECTS_MONITOR:= $(SOURCES_MONITOR:.c=.o)
LDFLAGS_MONITOR:= $(BIN_LDFLAGS)

LIB_STATIC:= libxcache.a
SOURCES_LIB:= remote.c server.c  murmur3_hash.c xcache.c mem.c conf.c
OBJECTS_LIB:= $(SOURCES_LIB:.c=.o)


all: $(SOURCES) $(BIN) $(TEST) $(MONITOR) $(SOURCES_LIB) $(LIB_STATIC)


$(BIN) : $(OBJECTS_BIN) $(LIB_STATIC)
	$(CC) -o $@ $(OBJECTS_BIN) $(BIN_LDFLAGS)

$(TEST) : $(OBJECTS_TEST) $(LIB_STATIC)
	$(CC) -o $@ $(OBJECTS_TEST) $(LDFLAGS_TEST)

$(MONITOR) : $(OBJECTS_MONITOR) $(LIB_STATIC)
	$(CC) -o $@ $(OBJECTS_MONITOR) $(LDFLAGS_TEST)

$(LIB_STATIC): $(OBJECTS_LIB)
	$(AR) -r $@ $^

clean:
	rm -rf $(OBJECTS_BIN) $(OBJECTS_TEST) $(OBJECTS_MONITOR) $(OBJECTS_LIB) $(BIN) $(TEST) $(MONITOR) $(LIB_STATIC)

clean-obj:
	rm -rf $(OBJECTS_BIN) $(OBJECTS_TEST) $(OBJECTS_MONITOR) $(OBJECTS_LIB)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<
