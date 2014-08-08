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
GTEST_CC_SRCS = 3rdparty/gtest/fused-src/gtest/gtest-all.cc
JLIB_CC_SRCS = $(wildcard 3rdparty/JLib/*.cc)
HOOK_CC_SRCS = $(wildcard 3rdparty/minhook/src/*.cc) $(wildcard 3rdparty/d3dhook/*.cc)
HOOK_C_SRCS = $(wildcard 3rdparty/minhook/src/hde32/*.c)
CONTRIB_CC_SRCS = $(GTEST_CC_SRCS) $(JLIB_CC_SRCS)
CONTRIB_C_SRCS = $(wildcard 3rdparty/*.c)

# Main program sources
BASE_CPP_SRCS = $(wildcard *.cpp) $(wildcard lib/*.cpp)
MAIN_CPP_SRCS = targets/Main.cpp $(wildcard tests/*.cpp) $(BASE_CPP_SRCS)
DLL_CPP_SRCS = $(wildcard targets/Dll*.cpp) $(BASE_CPP_SRCS)
LAUNCHER_CPP_SRCS = targets/Launcher.cpp

NON_GEN_SRCS = *.cpp lib/*.cpp targets/*.cpp tests/*.cpp
NON_GEN_HEADERS = $(filter-out lib/Version.h, $(filter-out lib/Protocol.%.h, \
	$(wildcard lib/*.h tests/*.h targets/*.h *.h)))
AUTOGEN_HEADERS = lib/Version.h lib/Protocol.*.h

# Main program objects
MAIN_OBJECTS = $(MAIN_CPP_SRCS:.cpp=.o) $(CONTRIB_CC_SRCS:.cc=.o) $(CONTRIB_C_SRCS:.c=.o)
DLL_OBJECTS = $(DLL_CPP_SRCS:.cpp=.o) $(HOOK_CC_SRCS:.cc=.o) $(HOOK_C_SRCS:.c=.o) $(CONTRIB_C_SRCS:.c=.o)

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
    ASTYLE = 3rdparty/astyle.exe
else
    CHMOD_X = chmod +x $@
    GRANT =
    ASTYLE = 3rdparty/astyle
endif

# Build flags
DEFINES = -DWIN32_LEAN_AND_MEAN -D_M_IX86 -DNAMED_PIPE='"\\\\.\\pipe\\cccaster_pipe"' -DMBAA_EXE='"$(MBAA_EXE)"'
DEFINES += -DBINARY='"$(BINARY)"' -DHOOK_DLL='"$(DLL)"' -DLAUNCHER='"$(LAUNCHER)"' -DFOLDER='"$(FOLDER)/"'
INCLUDES = -I$(CURDIR) -I$(CURDIR)/lib -I$(CURDIR)/tests -I$(CURDIR)/3rdparty -I$(CURDIR)/3rdparty/cereal/include
INCLUDES += -I$(CURDIR)/3rdparty/gtest/include -I$(CURDIR)/3rdparty/SDL2/include
INCLUDES += -I$(CURDIR)/3rdparty/minhook/include -I$(CURDIR)/3rdparty/d3dhook
CC_FLAGS = -m32 $(INCLUDES) $(DEFINES)

# Linker flags
LD_FLAGS = -m32 -static -L$(CURDIR)/3rdparty/SDL2/build -L$(CURDIR)/3rdparty/SDL2/build/.libs
LD_FLAGS += -lSDL2 -lSDL2main -lws2_32 -lwinmm -lwinpthread -ldinput8 -lgdi32 -limm32 -lole32 -loleaut32 -lversion

# Build options
DEFINES += -DENABLE_LOGGING
# DEFINES += -DLOGGER_MUTEXED
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
	$(ZIP) $(NAME).v$(VERSION).zip $^
	$(GRANT)
	rm -rf $(FOLDER)
	@echo
	if [ -s scripts/deploy ]; then scripts/deploy; fi;

$(FOLDER):
	@mkdir $(FOLDER)

$(BINARY): sdl $(MAIN_OBJECTS) res/icon.res
	$(CXX) -o $@ $(CC_FLAGS) -Wall -std=c++11 $(MAIN_OBJECTS) res/icon.res $(LD_FLAGS)
	@echo
	$(STRIP) $@
	$(CHMOD_X)

$(DLL): sdl $(DLL_OBJECTS) $(FOLDER)
	$(CXX) -o $@ $(CC_FLAGS) -Wall -std=c++11 $(DLL_OBJECTS) -shared $(LD_FLAGS) -ld3dx9
	@echo
	$(STRIP) $@
	$(GRANT)

$(LAUNCHER): $(LAUNCHER_CPP_SRCS) $(FOLDER)
	$(CXX) -o $@ $(LAUNCHER_CPP_SRCS) -m32 -s -Os -O2 -Wall -static -mwindows
	@echo
	$(STRIP) $@
	$(CHMOD_X)

res/icon.res: res/icon.rc res/icon.ico
	$(WINDRES) -F pe-i386 res/icon.rc -O coff -o $@

define make_version
@printf "#define COMMIT_ID \"`git rev-parse HEAD`\"\n\
#define BUILD_TIME \"`date`\"\n\
#define VERSION \"$(VERSION)\"" > lib/Version.h
endef

define make_protocol
@scripts/make_protocol $(NON_GEN_HEADERS)
endef

.depend: $(NON_GEN_SRCS) $(NON_GEN_HEADERS)
	$(make_version)
	$(make_protocol)
	@echo "Regenerating .depend"
	@$(CXX) $(CC_FLAGS) -std=c++11 -MM *.cpp > $@
	@$(CXX) $(CC_FLAGS) -std=c++11 -MM lib/*.cpp     | sed -r "s/^([A-Za-z]+\.o\: )/lib\/\1/"     >> $@
	@$(CXX) $(CC_FLAGS) -std=c++11 -MM targets/*.cpp | sed -r "s/^([A-Za-z]+\.o\: )/targets\/\1/" >> $@
	@$(CXX) $(CC_FLAGS) -std=c++11 -MM tests/*.cpp   | sed -r "s/^([A-Za-z]+\.o\: )/tests\/\1/"   >> $@

protocol: $(NON_GEN_HEADERS)
	$(make_protocol)

version:
	$(make_version)

autogen: protocol version

sdl:
	make --jobs --directory 3rdparty/SDL2 CFLAGS="-m32 -ggdb3 -O0 -fno-inline"
	@echo

sdl_release:
	make --jobs --directory 3rdparty/SDL2 CFLAGS="-m32 -s -Os -O3"
	@echo

sdl_profile:
	make --jobs --directory 3rdparty/SDL2 CFLAGS="-m32 -O3 -fno-rtti -pg"
	@echo

sdl_clean:
	make --directory 3rdparty/SDL2 clean

clean:
	rm -f $(AUTOGEN_HEADERS) .depend *.res *.exe *.dll *.zip *.o lib/*.o targets/*.o tests/*.o
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
    $(filter-out AsmHacks.h, $(NON_GEN_SRCS) $(NON_GEN_HEADERS))

count:
	wc -l $(NON_GEN_SRCS) $(NON_GEN_HEADERS)

.PHONY: clean check trim format count deploy autogen protocol version sdl sdl_release sdl_profile sdl_clean

ifeq (,$(findstring clean, $(MAKECMDGOALS)))
ifeq (,$(findstring check, $(MAKECMDGOALS)))
ifeq (,$(findstring trim, $(MAKECMDGOALS)))
ifeq (,$(findstring format, $(MAKECMDGOALS)))
ifeq (,$(findstring count, $(MAKECMDGOALS)))
ifeq (,$(findstring deploy, $(MAKECMDGOALS)))
ifeq (,$(findstring autogen, $(MAKECMDGOALS)))
ifeq (,$(findstring version, $(MAKECMDGOALS)))
ifeq (,$(findstring protocol, $(MAKECMDGOALS)))
ifeq (,$(findstring sdl, $(MAKECMDGOALS)))
-include .depend
endif
endif
endif
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
