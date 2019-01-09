
SHELL := /bin/bash # Use bash syntax

# Directories included with the source code
IDIR = ./include
SDIR = ./src
TDIR = ./test
# Directories generated
ODIR = ./obj
LDIR = ./lib

AR = ar
CC = gcc
CFLAGS = -Wall -Wextra -Werror
OPS = -g -O2
IFLAGS = -I$(IDIR)

LIBS =

SRC = $(wildcard $(SDIR)/*.c)
OBJS = $(patsubst $(SDIR)/%.c, $(ODIR)/%.o, $(SRC))


.PHONY: dirs
dirs:
	@mkdir -p $(ODIR) $(LDIR) || echo "FAILED TO MAKE DIRECTORIES!"

$(ODIR)/%.o: $(SDIR)/%.c dirs
	$(CC) $(CFLAGS) $(IFLAGS) $(OPS) -c $< -o $@

all: $(OBJS)
	echo "TODO create a static or dynamic library"
#	$(AR) 

.PHONY: test
test: test_simple test_linear test_nospace test_random test_large

.PHONY: test_simple
test_simple: $(OBJS)
	$(CC) $(OPS) $(CFLAGS) $(IFLAGS) $(LIBS) $^ $(TDIR)/simple.c -o $(TDIR)/simple.o
	@echo "START SIMPLE TEST"
	@$(TDIR)/simple.o && echo "PASSED" || echo "FAILED"

.PHONY: test_linear
test_linear: $(OBJS)
	$(CC) $(OPS) $(CFLAGS) $(IFLAGS) $(LIBS) $^ $(TDIR)/linear.c -o $(TDIR)/linear.o
	@echo "START LINEAR TEST"
	@$(TDIR)/linear.o && echo "PASSED" || echo "FAILED"

.PHONY: test_nospace
test_nospace: $(SRC)
	$(CC) $(OPS) $(CFLAGS) $(IFLAGS) $(LIBS) -DTEST_HASHMAP_NOSPACE $^ $(TDIR)/nospace.c -o $(TDIR)/nospace.o
	@echo "START NOSPACE TEST"
	@$(TDIR)/nospace.o && echo "PASSED" || echo "FAILED"

.PHONY: test_random
test_random: $(OBJS)
	$(CC) $(OPS) $(CFLAGS) $(IFLAGS) $(LIBS) $^ $(TDIR)/random.c -o $(TDIR)/random.o
	@echo "START RANDOM TEST"
	@$(TDIR)/random.o && echo "PASSED" || echo "FAILED"

.PHONY: test_large
test_large: $(OBJS)
	$(CC) $(OPS) $(CFLAGS) $(IFLAGS) $(LIBS) $^ $(TDIR)/large.c -o $(TDIR)/large.o
	@echo "START LARGE RANDOM TEST"
	@$(TDIR)/large.o && echo "PASSED" || echo "FAILED"

.PHONY: test_speed_insert
test_speed_insert: $(OBJS)
	$(CC) $(OPS) $(CFLAGS) $(IFLAGS) $(LIBS) $^ $(TDIR)/speed_insert.c -o $(TDIR)/speed_insert.o
	@echo "START SPEED TEST"
	@$(TDIR)/speed_insert.o && echo "PASSED" || echo "FAILED"

.PHONY: misc_checks
misc_checks: $(OBJS)
	$(CC) $(OPS) $(CFLAGS) $(IFLAGS) $(LIBS) $^ $(TDIR)/checks.c -o $(TDIR)/checks.o
	@echo "START CHECKS"
	@$(TDIR)/checks.o && echo "DONE" || echo "FAILED"

.PHONY: clean
clean:
	@rm -rf $(ODIR) $(LDIR) $(TDIR)/*.o && echo "CLEANED!" || echo "FAILED TO CLEANUP!"

