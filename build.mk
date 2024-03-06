obj += ini.o

# bin += ini
# ini-obj += $(obj)

bin += test/test
test/test-obj += $(obj)
test/test-obj += test/test.o

cflags += -O2 -g0 -mavx -mavx2
lflags += -no-pie
