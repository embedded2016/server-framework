EXEC = test-async test-reactor
.PHONY: all
all: $(EXEC)

CC ?= gcc
CFLAGS = -std=gnu99 -Wall -O2 -g -I .
LDFLAGS = -lpthread

OBJS := \
	async.o \
	reactor.o
deps := $(OBJS:%.o=%.o.d)

test-%: %.o tests/test-%.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ -MMD -MF $@.d $<

clean:
	$(RM) $(EXEC) $(OBJS) $(deps)

-include $(deps)
