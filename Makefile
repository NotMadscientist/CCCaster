# Main program
BINARY = cccaster.exe

# Main program sources
CPP_SRCS = $(wildcard *.cpp)
NON_GEN_HEADERS = $(filter-out Version.h, $(filter-out Util.string.h, $(filter-out Protocol.%.h, $(wildcard *.h))))

# Library sources
GTEST_SRCS = contrib/gtest/fused-src/gtest/gtest-all.cc
LIB_CPP_SRCS = $(GTEST_SRCS)
LIB_C_CSRCS = $(wildcard contrib/*.c)

# Tool chain
PREFIX = i686-w64-mingw32-
GCC = $(PREFIX)gcc
CXX = $(PREFIX)g++
WINDRES = $(PREFIX)windres
STRIP = $(PREFIX)strip
ZIP = zip
MSBUILD = C:/Windows/Microsoft.NET/Framework/v4.0.30319/MSBuild.exe

ifeq ($(OS),Windows_NT)
    CHMOD_X_BINARY = icacls $(BINARY) /grant Everyone:F
    ASTYLE = contrib/astyle.exe
else
    CHMOD_X_BINARY = chmod +x $(BINARY)
    ASTYLE = contrib/astyle
endif

# Build flags
DEFINES =
INCLUDES = -Icontrib -Icontrib/cereal/include -Icontrib/gtest/include
CC_FLAGS = -m32 -s $(INCLUDES) $(DEFINES)
LD_FLAGS = -m32 -static -lws2_32 -lmingw32 -lwinmm -lwinpthread
LD_FLAGS += -ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 -limm32 -lole32 -loleaut32 -lshell32 -lversion -luuid

OBJECTS = $(CPP_SRCS:.cpp=.o) $(LIB_CPP_SRCS:.cc=.o) $(LIB_C_CSRCS:.c=.o)
BUILD_TYPE = Debug
NUM_TO_STRING_ARGS = 10

all: STRIP = touch
all: DEFINES += -D_GLIBCXX_DEBUG
all: CC_FLAGS += -ggdb3 -O0 -fno-inline
all: $(BINARY)

release: DEFINES += -DNDEBUG -DRELEASE
release: CC_FLAGS += -Os -O2 -fno-rtti
release: BUILD_TYPE = Release
release: $(BINARY)

$(BINARY): Version.h Util.string.h protocol .depend $(OBJECTS) icon.res
	@echo
	$(CXX) -o $@ $(OBJECTS) $(LD_FLAGS) icon.res
	@echo
	$(STRIP) $@
	$(CHMOD_X_BINARY)

icon.res: icon.rc icon.ico
	@echo
	$(WINDRES) -F pe-i386 icon.rc -O coff -o $@

protocol:
	@./make_protocol $(NON_GEN_HEADERS)

Util.string.h:
	@./make_util_string $(NUM_TO_STRING_ARGS) > $@

Version.h:
	@date +"#define BUILD %s" > $@

.depend:
	@./make_protocol $(NON_GEN_HEADERS)
	@./make_util_string $(NUM_TO_STRING_ARGS) > Util.string.h
	@date +"#define BUILD %s" > Version.h
	@echo "Regenerating .depend ..."
	@$(CXX) $(CC_FLAGS) -std=c++11 -MM *.cpp > $@
	@echo

.PHONY: clean check trim format count Version.h protocol

clean:
	rm -f Version.h Util.string.h Protocol.*.h .depend *.res *.exe *.zip *.o $(OBJECTS)

check:
	cppcheck --enable=all *.cpp *.h

trim:
	sed --binary --in-place 's/\\r$$//' *.cpp $(NON_GEN_HEADERS)
	sed --in-place 's/[[:space:]]\\+$$//' *.cpp $(NON_GEN_HEADERS)

format:
	$(ASTYLE)              \
    --indent=spaces=4           \
    --convert-tabs              \
    --indent-preprocessor       \
    --indent-switches           \
    --style=allman              \
    --max-code-length=120       \
    --pad-paren                 \
    --pad-oper                  \
    --suffix=none               \
    --formatted                 \
    --keep-one-line-blocks      \
    --align-pointer=name        \
    --align-reference=type      \
    *.cpp $(NON_GEN_HEADERS)

count:
	wc -l *.cpp $(NON_GEN_HEADERS)

ifeq (,$(findstring clean, $(MAKECMDGOALS)))
ifeq (,$(findstring check, $(MAKECMDGOALS)))
ifeq (,$(findstring trim, $(MAKECMDGOALS)))
ifeq (,$(findstring format, $(MAKECMDGOALS)))
ifeq (,$(findstring count, $(MAKECMDGOALS)))
-include .depend
endif
endif
endif
endif
endif

%.o: %.cpp
	$(CXX) $(CC_FLAGS) -Wall -std=c++11 -o $@ -c $<

%.o: %.cc
	$(CXX) $(CC_FLAGS) -o $@ -c $<

%.o: %.c
	$(GCC) $(filter-out -fno-rtti, $(CC_FLAGS)) -Wno-attributes -o $@ -c $<
