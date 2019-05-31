
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
ifeq ($(prof), coverage)
PROF += -fprofile-arcs -ftest-coverage
endif
ifeq ($(prof), gprof)
PROF += -pg
endif
endif

OPS = -O2
ifdef ops
ifneq ($(strip $(ops)),)
	OPS = -O$(ops)
endif
endif

TESTFILE =simple
ifdef target
ifneq ($(strip $(target)),)
	TESTFILE =$(target)
endif
endif

DEFINES =
ifdef nospace
	DEFINES = -DTEST_HASHMAP_NOSPACE
endif

AR = ar
CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic -msse2 -g $(DEBUG) $(OPS) $(PROF)
IFLAGS = -I$(IDIR)

LIBS =

SRC = $(wildcard $(SDIR)/*.c)
OBJS = $(patsubst $(SDIR)/%.c, $(ODIR)/%.o, $(SRC))


.PHONY: dirs
dirs:
	@mkdir -p $(ODIR) $(LDIR) || echo "FAILED TO MAKE DIRECTORIES!"

$(ODIR)/%.o: $(SDIR)/%.c dirs
	$(CC) $(CFLAGS) $(IFLAGS) $(DEFINES) -c $< -o $@

all: $(OBJS)
	ar rcs $(LDIR)/libhashmap.a $(OBJS)
	$(CC) $(CFLAGS) $(IFLAGS) $(DEFINES) -c $(SRC) -shared -o $(LDIR)/libhashmap.so

.PHONY: test
test: $(OBJS)
	$(CC) $(CFLAGS) $(IFLAGS) $(DEFINES) $(LIBS) $^ $(TDIR)/$(TESTFILE).c -o $(TDIR)/$(TESTFILE).o
	@echo "START TEST: $(TESTFILE)"
	@$(TDIR)/$(TESTFILE).o && echo "PASSED" || echo "FAILED"
ifdef prof
ifeq ($(prof), coverage)
	@gcov -m hashmap.c
	@lcov --capture --directory obj --output-file main_coverage.info
	@genhtml main_coverage.info --output-directory out
endif
ifeq ($(prof), gprof)
	gprof $(TDIR)/$(TESTFILE).o gmon.out > analysis.txt
endif
endif

.PHONY: clean
clean:
	@rm -rf $(ODIR) $(LDIR) $(TDIR)/*.o out/ gmon.out *.info *.gcda *.gcno && echo "CLEANED!" || echo "FAILED TO CLEANUP!"

