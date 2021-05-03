CC = clang

CFLAGS = -fsanitize=address -O0 -g -Iinclude -I/usr/local/include -L/usr/local/lib -lsodium -luv  -Wall -Wno-unused-command-line-argument -pthread
SRC_FILES = tuple util/buffer util/map util/list block transaction blockchain util/guid network message settings message_history miner pool rest util/json util/http
OBJ_FILES = $(addprefix obj/,$(SRC_FILES:=.o))

MAIN = blockchaindb main
MAIN_BINS = $(addprefix bin/,$(MAIN))
TEST_BINS = $(addprefix bin/test_suite_,$(SRC_FILES))
LIBS = 

all: $(MAIN_BINS) $(TEST_BINS)

obj/%.o: src/%.c
	$(CC) -c $(CFLAGS) $^ -o $@
obj/%.o: main/%.c
	$(CC) -c $(CFLAGS) $^ -o $@
obj/%.o: tests/%.c
	$(CC) -c $(CFLAGS) $^ -o $@

lib/libsodium: 
	
bin/blockchaindb: obj/blockchaindb.o $(OBJ_FILES)
	$(CC) $(CFLAGS) $(LIBS) $^ -o $@

bin/main: obj/main.o $(OBJ_FILES)
	$(CC) $(CFLAGS) $(LIBS) $^ -o $@

bin/test_suite_%: obj/test_suite_%.o $(OBJ_FILES)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	find obj/ ! -name .gitignore -type f -delete 
	find bin/ ! -name .gitignore -type f -delete 

test: $(TEST_BINS)
	for f in $(TEST_BINS); do echo $$f; $$f; echo; done

.PHONY: all clean test