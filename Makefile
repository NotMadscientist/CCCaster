# Main program
BINARY = CCCaster.exe

# Main program sources
CPP_SRCS = $(wildcard *.cpp)

# Library sources
NETLINK_SRCS = $(wildcard contrib/netLink/src/*.cc)
LIB_CPP_SRCS = $(NETLINK_SRCS)
LIB_C_CSRCS =

# Tool chain
GCC = gcc
CXX = g++
STRIP = strip
ZIP = zip
MSBUILD = C:/Windows/Microsoft.NET/Framework/v4.0.30319/MSBuild.exe
ASTYLE = contrib/astyle.exe
BUILD_ENV = mingw-builds

# Build flags
DEFINES =
CC_FLAGS = -m32 -s -Icontrib -Icontrib/netLink/include $(DEFINES)
LD_FLAGS = -m32 -static -lws2_32 -lmingw32
LD_FLAGS += -ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 -limm32 -lole32 -loleaut32 -lshell32 -lversion -luuid

ifeq ($(BUILD_ENV),mingw-builds)
    LD_FLAGS += -lwinpthread
else
    LD_FLAGS += -lpthreadGC2
    DEFINES += -D_WIN32_WINNT=0x501 -DMISSING_CONSOLE_FONT_SIZE
endif

OBJECTS = $(CPP_SRCS:.cpp=.o) $(LIB_CPP_SRCS:.cc=.o) $(LIB_C_CSRCS:.c=.o)
BUILD_TYPE = Debug

all: STRIP = touch
all: DEFINES += -D_GLIBCXX_DEBUG
all: CC_FLAGS += -g
all: binary

binary: $(BINARY)
depend: .depend

$(BINARY): depend $(OBJECTS) icon.res
	@echo
	$(CXX) -o $@ $(OBJECTS) $(LD_FLAGS) icon.res
	@echo
	$(STRIP) $(BINARY)
	icacls $(BINARY) /grant Everyone:F

icon.res: icon.rc icon.ico
	@echo
	windres -F pe-i386 icon.rc -O coff -o icon.res

Version.h:
	date +"#define BUILD %s" > Version.h

.PHONY: clean check trim format count

.depend: Version.h
	$(CXX) $(CC_FLAGS) -std=c++11 -MM *.cpp > .depend
	@echo

clean:
	rm -f Version.h .depend *.res *.o *.exe *.zip
	rm -rf contrib/netLink/src/*.o

check:
	cppcheck --enable=all *.cpp *h

trim:
	sed --binary --in-place 's/\\r$$//' *.cpp *h
	sed --in-place 's/[[:space:]]\\+$$//' *.cpp *h

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
    *.cpp *h

count:
	wc -l *.cpp *h

ifeq (,$(findstring clean,$(MAKECMDGOALS)))
ifeq (,$(findstring check,$(MAKECMDGOALS)))
ifeq (,$(findstring trim,$(MAKECMDGOALS)))
ifeq (,$(findstring format,$(MAKECMDGOALS)))
ifeq (,$(findstring count,$(MAKECMDGOALS)))
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
	$(GCC) $(CC_FLAGS) -Wno-attributes -o $@ -c $<
