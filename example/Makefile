
#export CCACHE_REMOTE_CONF="--PROXY=127.0.0.1:66666";
export CCACHE_REMOTE_CONF="$(shell ../server/xcache_monitor -G 127.0.0.1:66666)";
XC := ../client/xcache_client
CC := $(XC) gcc

SRCS := \
	main.c \
	add.c

CFLAGS := -g -Wall -w
LDFLAGS :=

OBJECTS := $(SRCS:.c=.o)

BIN := xcache_test

$(BIN): $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

.c.o:
	$(warning $(CCACHE_REMOTE_CONF))
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(BIN) $(OBJECTS)
