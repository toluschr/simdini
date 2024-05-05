CC := gcc

CFLAGS := -O3 -g0 -Werror -Wall -Wextra
CFLAGS += -Wno-error=maybe-uninitialized
CFLAGS += -MMD
CFLAGS += -march=native
CFLAGS += -mtune=native

obj = ini.o
dep = $(obj:%.o=%.d)

.PHONY: all
all: ini.o

clean:
	-rm -f $(obj) $(dep)

$(obj): %.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

-include $(dep)
