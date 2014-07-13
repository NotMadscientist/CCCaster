VERSION = 3.0
NAME = cccaster

# Main programs
ARCHIVE = $(NAME).v$(VERSION).zip
BINARY = $(NAME).v$(VERSION).exe
FOLDER = $(NAME)
DLL = $(FOLDER)/hook.dll
LAUNCHER = $(FOLDER)/launcher.exe
MBAA_EXE = MBAA.exe

# Library sources
GTEST_CC_SRCS = contrib/gtest/fused-src/gtest/gtest-all.cc
JLIB_CC_SRCS = $(wildcard contrib/JLib/*.cc)
CONTRIB_CC_SRCS = $(GTEST_CC_SRCS) $(JLIB_CC_SRCS)
CONTRIB_C_CSRCS = $(wildcard contrib/*.c)

# Main program sources
MAIN_CPP_SRCS = targets/Main.cpp $(wildcard *.cpp) $(wildcard tests/*.cpp)
DLL_CPP_SRCS = targets/DllMain.cpp $(wildcard *.cpp)
LAUNCHER_CPP_SRCS = targets/Launcher.cpp

NON_GEN_SRCS = *.cpp targets/*.cpp tests/*.cpp
NON_GEN_HEADERS = $(filter-out Version.h, $(filter-out Protocol.%.h, $(wildcard tests/*.h *.h)))

# Main program objects
MAIN_OBJECTS = $(MAIN_CPP_SRCS:.cpp=.o) $(CONTRIB_CC_SRCS:.cc=.o) $(CONTRIB_C_CSRCS:.c=.o)
DLL_OBJECTS = $(DLL_CPP_SRCS:.cpp=.o) $(CONTRIB_C_CSRCS:.c=.o)

# Tool chain
PREFIX = i686-w64-mingw32-
GCC = $(PREFIX)gcc
CXX = $(PREFIX)g++
WINDRES = $(PREFIX)windres
STRIP = $(PREFIX)strip
ZIP = zip

ifeq ($(OS),Windows_NT)
    CHMOD_X = icacls $@ /grant Everyone:F
    GRANT = icacls $@ /grant Everyone:F
    ASTYLE = contrib/astyle.exe
else
    CHMOD_X = chmod +x $@
    GRANT =
    ASTYLE = contrib/astyle
endif

# Build flags
DEFINES = -DWIN32_LEAN_AND_MEAN -DNAMED_PIPE='"\\\\.\\pipe\\cccaster_pipe"' -DMBAA_EXE='"$(MBAA_EXE)"'
DEFINES += -DBINARY='"$(BINARY)"' -DHOOK_DLL='"$(DLL)"' -DLAUNCHER='"$(LAUNCHER)"' -DFOLDER='"$(FOLDER)/"'
INCLUDES = -I$(CURDIR) -I$(CURDIR)/Tests -I$(CURDIR)/contrib -I$(CURDIR)/contrib/cereal/include
INCLUDES += -I$(CURDIR)/contrib/gtest/include -I$(CURDIR)/contrib/SDL2
CC_FLAGS = -m32 $(INCLUDES) $(DEFINES)
LD_FLAGS = -m32 -static -Lcontrib/SDL2 -lSDL2 -lSDL2main -lws2_32 -lwinmm -lwinpthread -ldinput8 -ldxguid -ldxerr8
LD_FLAGS += -luser32 -lgdi32 -limm32 -lole32 -loleaut32 -lshell32 -lversion -luuid

# Build options
DEFINES += -DENABLE_LOGGING
# DEFINES += -DJLIB_MUTEXED

all: STRIP = touch
all: DEFINES += -D_GLIBCXX_DEBUG
all: CC_FLAGS += -ggdb3 -O0 -fno-inline
all: $(ARCHIVE)

release: DEFINES += -DNDEBUG -DRELEASE
release: CC_FLAGS += -s -Os -O2 -fno-rtti
release: $(ARCHIVE)

profile: STRIP = touch
profile: DEFINES += -DNDEBUG -DRELEASE
profile: CC_FLAGS += -O2 -fno-rtti -pg
profile: LD_FLAGS += -pg -lgmon
profile: $(ARCHIVE)

$(ARCHIVE): $(BINARY) $(DLL) $(LAUNCHER)
	@echo
	$(ZIP) $(NAME).v$(VERSION).zip $^
	$(GRANT)
	@echo
	if [ -s ./deploy ]; then ./deploy; fi;

$(FOLDER):
	@mkdir $(FOLDER)

$(BINARY): protocol $(MAIN_OBJECTS) icon.res
	@echo
	$(CXX) -o $@ $(CC_FLAGS) -Wall -std=c++11 $(MAIN_OBJECTS) icon.res $(LD_FLAGS)
	@echo
	$(STRIP) $@
	$(CHMOD_X)

$(DLL): protocol $(DLL_OBJECTS) $(FOLDER)
	@echo
	$(CXX) -o $@ $(CC_FLAGS) -Wall -std=c++11 $(DLL_OBJECTS) -shared $(LD_FLAGS)
	@echo
	$(STRIP) $@
	$(GRANT)

$(LAUNCHER): $(LAUNCHER_CPP_SRCS) $(FOLDER)
	@echo
	$(CXX) -o $@ $(LAUNCHER_CPP_SRCS) -m32 -s -Os -O2 -Wall -static -mwindows
	@echo
	$(STRIP) $@
	$(CHMOD_X)

icon.res: icon.rc icon.ico
	@echo
	$(WINDRES) -F pe-i386 icon.rc -O coff -o $@

.depend: $(NON_GEN_SRCS)
	@echo "Regenerating .depend"
	@$(CXX) $(CC_FLAGS) -std=c++11 -MM $(NON_GEN_SRCS) > $@
	@echo

protocol:
	@./make_protocol $(NON_GEN_HEADERS)

Version.h:
	@echo "Regenerating Version.h"
	@printf "#define COMMIT_ID \"`git rev-parse HEAD`\"\n\
	#define BUILD_TIME \"`date`\"\n\
	#define VERSION \"$(VERSION)\"" > $@

autogen: protocol Version.h

.PHONY: clean check trim format count deploy autogen protocol Version.h

clean:
	rm -f Version.h Protocol.*.h .depend *.res *.exe *.dll *.zip *.o targets/*.o tests/*.o
	rm -f $(MAIN_OBJECTS) $(DLL_OBJECTS)
	rm -rf $(FOLDER)

check:
	cppcheck --enable=all $(NON_GEN_SRCS) $(NON_GEN_HEADERS)

trim:
	sed --binary --in-place 's/\\r$$//' $(NON_GEN_SRCS) $(NON_GEN_HEADERS)
	sed --in-place 's/[[:space:]]\\+$$//' $(NON_GEN_SRCS) $(NON_GEN_HEADERS)

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
    $(NON_GEN_SRCS) $(NON_GEN_HEADERS)

count:
	wc -l $(NON_GEN_SRCS) $(NON_GEN_HEADERS)

ifeq (,$(findstring clean, $(MAKECMDGOALS)))
ifeq (,$(findstring check, $(MAKECMDGOALS)))
ifeq (,$(findstring trim, $(MAKECMDGOALS)))
ifeq (,$(findstring format, $(MAKECMDGOALS)))
ifeq (,$(findstring count, $(MAKECMDGOALS)))
ifeq (,$(findstring deploy, $(MAKECMDGOALS)))
ifeq (,$(findstring autogen, $(MAKECMDGOALS)))
-include .depend
endif
endif
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
