
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
CFLAGS = -Wall -Wextra -Werror -pedantic -msse2 -g $(DEBUG) $(OPS) $(PROF)
IFLAGS = -I$(IDIR)
LIBS =

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

DEFINES =
TESTFILE =simple
ifdef target
ifneq ($(strip $(target)),)
TESTFILE =$(target)
endif
ifeq ($(strip $(target)), nospace)
DEFINES += -DTEST_HASHMAP_NOSPACE
endif
endif

ifdef debug
DEFINES += -DDEBUG
endif

ifdef invariant
DEFINES += -DINVARIANT
endif

ifdef cpp
CC = g++
endif

SRC = $(wildcard $(SDIR)/*.c)
OBJS = $(patsubst $(SDIR)/%.c, $(ODIR)/%.o, $(SRC))
UTILOBJ = $(TDIR)/util.o


all: $(OBJS)
	ar rcs $(LDIR)/libhashmap.a $(OBJS)
	$(CC) $(CFLAGS) $(IFLAGS) $(DEFINES) -c $(SRC) -shared -o $(LDIR)/libhashmap.so

.PHONY: dirs
dirs:
	@mkdir -p $(ODIR) $(LDIR) || echo "FAILED TO MAKE DIRECTORIES!"

$(ODIR)/%.o: $(SDIR)/%.c dirs
	$(CC) $(CFLAGS) $(IFLAGS) $(DEFINES) -c $< -o $@

$(TDIR)/%.o: $(TDIR)/%.c
	$(CC) $(CFLAGS) $(IFLAGS) $(DEFINES) -c $< -o $@

.PHONY: check
check:
	@echo "START CPPCHECK"
	cppcheck --enable=warning,style,performance,portability,information -I $(IDIR) $(SRC)
	@echo "END CPPCHECK"
	@echo "START CLANG/SCAN-BUILD"
	scan-build make
	@echo "END CLANG/SCAN-BUILD"

.PHONY: test
test: $(OBJS) $(UTILOBJ)
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

