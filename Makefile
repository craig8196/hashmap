
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
PROF += -fprofile-arcs -ftest-coverage --coverage \
		-fno-inline -fno-inline-small-functions -fno-default-inline
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

ifdef prof
# Additional inlining prevention.
	OPS = -O0
endif

AR = ar
CC = g++
CFLAGS = -Wall -Wextra -Werror -pedantic -msse2 -g $(DEBUG) $(OPS) $(PROF)
IFLAGS = -I$(IDIR)
LIBS =

DEFINES =
TESTFILE =prove
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

ifdef verbose
DEFINES += -DVERBOSE
endif

ifdef reserve
DEFINES += -DALLOW_RESERVE
endif

ifdef hashmap
ifneq ($(strip $(hashmap)),)
DEFINES += -D$(hashmap)
endif
else
DEFINES += -DHASHMAP
endif

ifdef compiler
ifneq ($(strip $(compiler)),)
CC = $(compiler)
endif
endif

ifeq ($(strip $(CC)),gcc)
EXT = c
else
EXT = cpp
endif

ifdef inline
CFLAGS += -finline-functions
endif

ifndef seed
seed =
endif
ifdef seed
ifneq ($(strip $(seed)),)
DEFINES += -DFORCESEED=$(seed)
else
DEFINES += -DFORCESEED=0
endif
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

.PHONY: doc
doc:
	doxygen Doxyfile

.PHONY: test
test: $(OBJS) $(UTILOBJ)
	$(CC) $(CFLAGS) $(IFLAGS) $(DEFINES) $(LIBS) $^ $(TDIR)/$(TESTFILE).$(EXT) -o $(TDIR)/$(TESTFILE).o
	@echo "START TEST: $(TESTFILE)"
ifdef prof
ifeq ($(prof), coverage)
	@lcov -c -i -b . -d . -o Coverage.baseline
endif
endif
	@$(TDIR)/$(TESTFILE).o && echo "PASSED" || echo "FAILED"
ifdef prof
ifeq ($(prof), coverage)
	@lcov -c -b . -d . -o Coverage.out
	@lcov -a Coverage.baseline -a Coverage.out -o Coverage.combined
	@genhtml Coverage.combined --output-directory out
endif
ifeq ($(prof), gprof)
	gprof $(TDIR)/$(TESTFILE).o gmon.out > analysis.txt
endif
endif

.PHONY: clean
clean:
	@rm -rf $(ODIR) $(LDIR) $(TDIR)/*.o out/ gmon.out *.txt *.info *.gcda *.gcno Coverage.* && echo "CLEANED!" || echo "FAILED TO CLEANUP!"

