OBJS =	framedisplay.o \
	mbaacc_framedisplay.o \
	mbaacc_character.o \
	mbaacc_framedata.o \
	mbaacc_cg.o \
	mbaacc_pack.o \
	texture.o \
	clone.o \
	render.o


ifeq ($(OS),Windows_NT)
	OPENGL_HEADERS = /usr/mingw/i686-w64-mingw32/include/GL
else
	OPENGL_HEADERS = /usr/i686-w64-mingw32/include/GL
endif


CXX = i686-w64-mingw32-g++ -m32

CFLAGS = -s -O3 -Os -Wall -I"$(CURDIR)" -I"$(CURDIR)/../libpng" -I"$(CURDIR)/../libz" -I"$(CURDIR)/../SDL" -I$(OPENGL_HEADERS)

LIBS = -static -lmingw32 -lSDLmain -lSDL -lpng -lz -lopengl32 -lcomctl32 -lole32 -lwinmm -ldxguid

LDFLAGS = -mwindows -L"$(CURDIR)/../libpng" -L"$(CURDIR)/../libz" -L"$(CURDIR)/../SDL"


all: test.exe

clean:
	rm -f *.o *.exe

.cc.o:
	$(CXX) $(CFLAGS) -o $@ -c $<

.cpp.o:
	$(CXX) $(CFLAGS) -o $@ -c $<

test.exe: $(OBJS) test.o
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) test.o $(LIBS)
