
SHELL := /bin/bash # Use bash syntax

# Directories included with the source code
IDIR = ./include
SDIR = ./src
TDIR = ./test
# Directories generated
ODIR = ./obj
LDIR = ./lib

PROF = 
ifdef prof
ifeq ($(prof), true)
PROF += -fprofile-arcs -ftest-coverage
endif
endif

OPS = -O2
ifdef ops
ifneq ($(strip $(ops)),)
	OPS = -O$(ops)
endif
endif

TESTFILE =simple
ifdef testtarget
ifneq ($(strip $(testtarget)),)
	TESTFILE =$(testtarget)
endif
endif

AR = ar
CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic -g $(DEBUG) $(OPS) $(PROF)
IFLAGS = -I$(IDIR)

LIBS =

SRC = $(wildcard $(SDIR)/*.c)
OBJS = $(patsubst $(SDIR)/%.c, $(ODIR)/%.o, $(SRC))


.PHONY: dirs
dirs:
	@mkdir -p $(ODIR) $(LDIR) || echo "FAILED TO MAKE DIRECTORIES!"

$(ODIR)/%.o: $(SDIR)/%.c dirs
	$(CC) $(CFLAGS) $(IFLAGS) -c $< -o $@

all: $(OBJS)
	ar rcs $(LDIR)/libhashmap.a $(OBJS)
	$(CC) $(CFLAGS) $(IFLAGS) -c $(SRC) -shared -o $(LDIR)/libhashmap.so

.PHONY: test
test: $(OBJS)
	$(CC) $(CFLAGS) $(IFLAGS) $(LIBS) $^ $(TDIR)/$(TESTFILE).c -o $(TDIR)/$(TESTFILE).o
	@echo "START TEST: $(TESTFILE)"
	@$(TDIR)/$(TESTFILE).o && echo "PASSED" || echo "FAILED"
ifdef prof
ifeq ($(prof), true)
	@gcov -m hashmap.c
	@lcov --capture --directory obj --output-file main_coverage.info
	@genhtml main_coverage.info --output-directory out
endif
endif

.PHONY: misc_checks
misc_checks: $(OBJS)
	$(CC) $(CFLAGS) $(IFLAGS) $(LIBS) $^ $(TDIR)/checks.c -o $(TDIR)/checks.o
	@echo "START CHECKS"
	@$(TDIR)/checks.o && echo "DONE" || echo "FAILED"

.PHONY: clean
clean:
	@rm -rf $(ODIR) $(LDIR) $(TDIR)/*.o out/ gmon.out *.info *.gcda *.gcno && echo "CLEANED!" || echo "FAILED TO CLEANUP!"

