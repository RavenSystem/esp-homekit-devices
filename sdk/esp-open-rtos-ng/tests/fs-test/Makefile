
CFLAGS += -std=gnu99

all: fs_test

fs_test: main.o fs_test.o
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -o $@

test: fs_test
	./fs_test > test.log

clean:
	rm -f *.o
	rm -f fs_test

.PHONY: all clean
