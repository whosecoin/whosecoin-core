CC = clang

CFLAGS = -fsanitize=address -O0 -g -Iinclude -I/usr/local/include -L/usr/local/lib -lsodium -luv  -Wall -Wno-unused-command-line-argument -pthread
SRC_FILES = tuple util/buffer util/map util/list block transaction blockchain util/guid network message settings message_history pool util/json util/http
OBJ_FILES = $(addprefix obj/,$(SRC_FILES:=.o))

MAIN = blockchaindb main
MAIN_BINS = $(addprefix bin/,$(MAIN))
TEST_BINS = $(addprefix bin/test_suite_,$(SRC_FILES))
LIBS = 

all: $(MAIN_BINS) $(TEST_BINS)

obj:
	mkdir obj
	mkdir obj/util

bin:
	mkdir bin

obj/%.o: src/%.c | obj
	$(CC) -c $(CFLAGS) $^ -o $@
obj/%.o: main/%.c | obj
	$(CC) -c $(CFLAGS) $^ -o $@
obj/%.o: tests/%.c | obj
	$(CC) -c $(CFLAGS) $^ -o $@
	
bin/blockchaindb: obj/blockchaindb.o $(OBJ_FILES) | bin
	$(CC) $(CFLAGS) $(LIBS) $^ -o $@

bin/main: obj/main.o $(OBJ_FILES) | bin
	$(CC) $(CFLAGS) $(LIBS) $^ -o $@

bin/test_suite_%: obj/test_suite_%.o $(OBJ_FILES) | bin
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -rf bin
	rm -rf obj

test: $(TEST_BINS)
	for f in $(TEST_BINS); do echo $$f; $$f; echo; done

.PHONY: all clean test