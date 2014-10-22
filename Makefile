VERSION = 2.1alpha
NAME = cccaster

# Main programs
ARCHIVE = $(NAME).v$(VERSION).zip
BINARY = $(NAME).v$(VERSION).exe
FOLDER = $(NAME)
DLL = $(FOLDER)/hook.dll
LAUNCHER = $(FOLDER)/launcher.exe
DEBUGGER = debugger.exe
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
MAIN_CPP_SRCS = $(wildcard targets/Main*.cpp) $(wildcard tests/*.cpp) $(BASE_CPP_SRCS)
DLL_CPP_SRCS = $(wildcard targets/Dll*.cpp) $(BASE_CPP_SRCS)

NON_GEN_SRCS = *.cpp targets/*.cpp lib/*.cpp tests/*.cpp
NON_GEN_HEADERS = \
	$(filter-out lib/Version.local.h,$(filter-out lib/Protocol.%.h,$(wildcard *.h targets/*.h lib/*.h tests/*.h)))
AUTOGEN_HEADERS = lib/Version.local.h lib/Protocol.*.h

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

# OS specific tools
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
# DEFINES += -DDISABLE_LOGGING
# DEFINES += -DLOGGER_MUTEXED
# DEFINES += -DJLIB_MUTEXED

INSTALL=1


all: debug

target-debug: STRIP = touch
target-debug: DEFINES += -D_GLIBCXX_DEBUG
target-debug: CC_FLAGS += -ggdb3 -O0 -fno-inline
target-debug: $(ARCHIVE)

target-release: DEFINES += -DNDEBUG -DRELEASE -DDISABLE_LOGGING
target-release: CC_FLAGS += -s -Os -O2 -fno-rtti
target-release: $(ARCHIVE)

target-release-logging: DEFINES += -DNDEBUG -DRELEASE
target-release-logging:: CC_FLAGS += -s -Os -O2 -fno-rtti
target-release-logging:: $(ARCHIVE)

target-profile: STRIP = touch
target-profile: DEFINES += -DNDEBUG -DRELEASE
target-profile: CC_FLAGS += -O2 -fno-rtti -pg
target-profile: LD_FLAGS += -pg -lgmon
target-profile: $(ARCHIVE)

debugger: $(DEBUGGER)


$(ARCHIVE): $(BINARY) $(DLL) $(LAUNCHER)
	@echo
	rm -f $(filter-out $(ARCHIVE),$(wildcard $(NAME)*.zip))
	$(ZIP) $(NAME).v$(VERSION).zip $^
	$(GRANT)

$(BINARY): $(MAIN_OBJECTS) res/icon.res
	rm -f $(filter-out $(BINARY),$(wildcard $(NAME)*.exe))
	$(CXX) -o $@ $(CC_FLAGS) -Wall -std=c++11 $(MAIN_OBJECTS) res/icon.res $(LD_FLAGS)
	@echo
	$(STRIP) $@
	$(CHMOD_X)
	@echo

$(DLL): $(DLL_OBJECTS)
	@mkdir -p $(FOLDER)
	$(CXX) -o $@ $(CC_FLAGS) -Wall -std=c++11 $(DLL_OBJECTS) -shared $(LD_FLAGS) -ld3dx9
	@echo
	$(STRIP) $@
	$(GRANT)
	@echo

$(LAUNCHER): targets/Launcher.cpp
	@mkdir -p $(FOLDER)
	$(CXX) -o $@ targets/Launcher.cpp -m32 -s -Os -O2 -Wall -static -mwindows
	@echo
	$(STRIP) $@
	$(CHMOD_X)
	@echo

$(DEBUGGER): targets/Debugger.cpp lib/Utilities.cpp lib/Logger.cpp
	$(CXX) -o $@ $^ -m32 -s -Os -O2 -Wall -static -std=c++11 \
		-I$(CURDIR) -Ilib -I3rdparty/cereal/include -I3rdparty/distorm3/include -L3rdparty/distorm3 -ldistorm3
	@echo
	$(STRIP) $@
	$(CHMOD_X)
	@echo

res/icon.res: res/icon.rc res/icon.ico
	$(WINDRES) -F pe-i386 res/icon.rc -O coff -o $@


define make_version
@scripts/make_version $(VERSION) > lib/Version.local.h
endef

define make_protocol
@scripts/make_protocol $(NON_GEN_HEADERS)
endef

define make_depend
@scripts/check_depend || ( echo "Regenerating .depend" \
&& $(CXX) $(CC_FLAGS) -std=c++11 -MM *.cpp         *.h                                                       > .depend \
&& $(CXX) $(CC_FLAGS) -std=c++11 -MM targets/*.cpp targets/*.h | sed -r "s/^([A-Za-z]+\.o\: )/targets\/\1/" >> .depend \
&& $(CXX) $(CC_FLAGS) -std=c++11 -MM lib/*.cpp     lib/*.h     | sed -r "s/^([A-Za-z]+\.o\: )/lib\/\1/"     >> .depend \
&& $(CXX) $(CC_FLAGS) -std=c++11 -MM tests/*.cpp   tests/*.h   | sed -r "s/^([A-Za-z]+\.o\: )/tests\/\1/"   >> .depend )
endef


version:
	$(make_version)

protocol:
	$(make_protocol)

depend: version protocol
	$(make_depend)

.depend: $(NON_GEN_SRCS) $(NON_GEN_HEADERS)
	$(make_version)
	$(make_protocol)
	$(make_depend)


sdl:
	@$(MAKE) --directory 3rdparty/SDL2 --environment-overrides CFLAGS='-m32 -ggdb3 -O0 -fno-inline'
	@echo

sdl-release:
	@$(MAKE) --directory 3rdparty/SDL2  CFLAGS='-m32 -s -Os -O3'
	@echo

sdl-clean:
	@$(MAKE) --directory 3rdparty/SDL2 clean


clean:
	rm -f $(AUTOGEN_HEADERS) *.res *.exe *.dll *.zip *.o targets/*.o lib/*.o tests/*.o
	rm -f $(MAIN_OBJECTS) $(DLL_OBJECTS)

clean-depend:
	rm -f .depend .include

clean-full: clean-depend clean
	rm -rf $(FOLDER)

check:
	cppcheck --enable=all $(NON_GEN_SRCS) $(NON_GEN_HEADERS)

trim:
	sed --binary --in-place 's/\\r$$//' $(NON_GEN_SRCS) $(NON_GEN_HEADERS)
	sed --in-place 's/[[:space:]]\\+$$//' $(NON_GEN_SRCS) $(NON_GEN_HEADERS)

format:
	$(ASTYLE)             \
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
    $(filter-out AsmHacks.h,$(NON_GEN_SRCS) $(NON_GEN_HEADERS))

count:
	@wc -l $(NON_GEN_SRCS) $(NON_GEN_HEADERS) | sort -nr | head -n 10 && echo '    ...'


ifeq (,$(findstring version,$(MAKECMDGOALS)))
ifeq (,$(findstring protocol,$(MAKECMDGOALS)))
ifeq (,$(findstring depend,$(MAKECMDGOALS)))
ifeq (,$(findstring clean,$(MAKECMDGOALS)))
ifeq (,$(findstring check,$(MAKECMDGOALS)))
ifeq (,$(findstring trim,$(MAKECMDGOALS)))
ifeq (,$(findstring format,$(MAKECMDGOALS)))
ifeq (,$(findstring count,$(MAKECMDGOALS)))
ifeq (,$(findstring install,$(MAKECMDGOALS)))
ifeq (,$(findstring sdl,$(MAKECMDGOALS)))
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


pre-build:
	$(make_version)
	$(make_protocol)
	@echo
	@echo ========== Main-build ==========
	@echo

post-build: main-build
	@echo
	@echo ========== Post-build ==========
	@echo
	if [ $(INSTALL) = 1 ] && [ -s ./scripts/install ]; then ./scripts/install; fi;


debug: post-build
release: post-build
release-logging: post-build
profile: post-build


ifneq (,$(findstring release-logging,$(MAKECMDGOALS)))
main-build: pre-build
	@$(MAKE) --no-print-directory target-release-logging
else
ifneq (,$(findstring release,$(MAKECMDGOALS)))
main-build: pre-build
	@$(MAKE) --no-print-directory target-release
else
ifneq (,$(findstring profile,$(MAKECMDGOALS)))
main-build: pre-build
	@$(MAKE) --no-print-directory target-profile
else
main-build: pre-build
	@$(MAKE) --no-print-directory target-debug
endif
endif
endif


%.o: %.cpp
	$(CXX) $(CC_FLAGS) -Wall -std=c++11 -o $@ -c $<

%.o: %.cc
	$(CXX) $(CC_FLAGS) -o $@ -c $<

%.o: %.c
	$(GCC) $(filter-out -fno-rtti,$(CC_FLAGS)) -Wno-attributes -o $@ -c $<
