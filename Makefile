# Main programs
BINARY = cccaster.exe
DLL = cccaster.dll
LAUNCHER = launcher.exe
FOLDER = cccaster/

# Main program sources
CPP_SRCS = $(wildcard *.cpp)
NON_GEN_HEADERS = $(filter-out Version.h, $(filter-out Protocol.%.h, $(wildcard *.h)))

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
    CHMOD_X = icacls $@ /grant Everyone:F
    ASTYLE = contrib/astyle.exe
else
    CHMOD_X = chmod +x $@
    ASTYLE = contrib/astyle
endif

# Build flags
DEFINES = -DWIN32_LEAN_AND_MEAN -DBINARY='"$(BINARY)"' -DFOLDER='"$(FOLDER)"'
INCLUDES = -Icontrib -Icontrib/cereal/include -Icontrib/gtest/include
CC_FLAGS = -m32 -s $(INCLUDES) $(DEFINES)
LD_FLAGS = -m32 -static -lws2_32 -lwinmm -lwinpthread -ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 -limm32
LD_FLAGS += -lole32 -loleaut32 -lshell32 -lversion -luuid

OBJECTS = $(CPP_SRCS:.cpp=.o) $(LIB_CPP_SRCS:.cc=.o) $(LIB_C_CSRCS:.c=.o)
BUILD_TYPE = Debug

all: STRIP = touch
all: DEFINES += -D_GLIBCXX_DEBUG
all: CC_FLAGS += -ggdb3 -O0 -fno-inline
all: $(BINARY) $(DLL) $(LAUNCHER)

release: DEFINES += -DNDEBUG -DRELEASE
release: CC_FLAGS += -Os -O2 -fno-rtti
release: BUILD_TYPE = Release
release: $(BINARY) $(DLL) $(LAUNCHER)

profile: STRIP = touch
profile: DEFINES += -DNDEBUG -DRELEASE
profile: CC_FLAGS += -Os -O2 -fno-rtti -pg
profile: LD_FLAGS += -pg -lgmon
profile: $(BINARY) $(DLL) $(LAUNCHER)

$(BINARY): Version.h protocol .depend $(OBJECTS) icon.res
	@echo
	$(CXX) -o $@ $(CC_FLAGS) -Wall -std=c++11 Main.cc $(OBJECTS) $(LD_FLAGS) icon.res
	@echo
	$(STRIP) $@
	$(CHMOD_X)

$(DLL): Version.h protocol .depend $(OBJECTS)
	@echo
	$(CXX) -o $@ $(CC_FLAGS) -Wall -std=c++11 DllMain.cc $(OBJECTS) -shared $(LD_FLAGS)
	@echo
	$(STRIP) $@
	$(CHMOD_X)

$(LAUNCHER): DllLauncher.cc
	@echo
	$(CXX) -o $@ $^ -m32 -s -Os -O2 -Wall -static -mwindows
	@echo
	$(STRIP) $@
	$(CHMOD_X)

icon.res: icon.rc icon.ico
	@echo
	$(WINDRES) -F pe-i386 icon.rc -O coff -o $@

depend:
	$(CXX) $(CC_FLAGS) -std=c++11 -MM *.cpp *.cc > .depend

.depend:
	@./make_protocol $(NON_GEN_HEADERS)
	@printf "#define COMMIT_ID \"`git rev-parse HEAD`\"\n#define BUILD_TIME \"`date`\"\n" > Version.h
	@echo "Regenerating .depend ..."
	@$(CXX) $(CC_FLAGS) -std=c++11 -MM *.cpp *.cc > $@
	@echo

protocol:
	@./make_protocol $(NON_GEN_HEADERS)

Version.h:
	@printf "#define COMMIT_ID \"`git rev-parse HEAD`\"\n#define BUILD_TIME \"`date`\"\n" > $@

.PHONY: clean check trim format count depend protocol Version.h

clean:
	rm -f Version.h Protocol.*.h .depend *.res *.exe *.dll *.zip *.o $(OBJECTS)

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
    *.cpp *.cc $(NON_GEN_HEADERS)

count:
	wc -l *.cpp *.cc $(NON_GEN_HEADERS)

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
