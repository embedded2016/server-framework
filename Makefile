EXEC = main
.PHONY: all
all: $(EXEC)

CC ?= gcc
CFLAGS = -std=gnu99 -Wall -O2 -g
LDFLAGS = -lpthread

OBJS := \
	async.o \
	main.o
deps := $(OBJS:%.o=%.o.d)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ -MMD -MF $@.d $<

main: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) $(EXEC) $(OBJS) $(deps)

-include $(deps)
