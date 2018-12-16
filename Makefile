
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
test: test_simple test_linear test_nospace test_random

.PHONY: test_simple
test_simple: $(OBJS)
	@echo "START SIMPLE TEST"
	$(CC) $(OPS) $(CFLAGS) $(IFLAGS) $(LIBS) $^ $(TDIR)/simple.c -o $(TDIR)/simple.o
	@$(TDIR)/simple.o && echo "PASSED" || echo "FAILED"

.PHONY: test_linear
test_linear: $(OBJS)
	@echo "START LINEAR TEST"
	$(CC) $(OPS) $(CFLAGS) $(IFLAGS) $(LIBS) $^ $(TDIR)/linear.c -o $(TDIR)/linear.o
	@$(TDIR)/linear.o && echo "PASSED" || echo "FAILED"

.PHONY: test_nospace
test_nospace: $(SRC)
	@echo "START NOSPACE TEST"
	$(CC) $(OPS) $(CFLAGS) $(IFLAGS) $(LIBS) -DTEST_HASHMAP_NOSPACE $^ $(TDIR)/nospace.c -o $(TDIR)/nospace.o
	@$(TDIR)/nospace.o && echo "PASSED" || echo "FAILED"

.PHONY: test_random
test_random: $(OBJS)
	@echo "START RANDOM TEST"
	$(CC) $(OPS) $(CFLAGS) $(IFLAGS) $(LIBS) $^ $(TDIR)/random.c -o $(TDIR)/random.o
	@$(TDIR)/random.o && echo "PASSED" || echo "FAILED"

.PHONY: clean
clean:
	@rm -rf $(ODIR) $(LDIR) $(TDIR)/*.o && echo "CLEANED!" || echo "FAILED TO CLEANUP!"

