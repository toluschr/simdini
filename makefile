CC := gcc

CFLAGS := -O3 -g0 -Werror -Wall -Wextra
CFLAGS += -Wno-error=maybe-uninitialized
CFLAGS += -march=native
CFLAGS += -mtune=native

obj = ini.o test.o
dep = $(obj:%.o=%.d)

.PHONY: all, clean
all: ini.o test

test: ini.o test.o
	$(CC) $(CFLAGS) $^ -o $@

clean:
	-rm -f $(obj) $(dep)

$(obj): %.o: %.c makefile
	$(CC) -MD $(CFLAGS) -c $< -o $@

-include $(dep)
