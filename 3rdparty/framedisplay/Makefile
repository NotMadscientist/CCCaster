OBJS =	framedisplay.o \
	mbaacc_framedisplay.o \
	mbaacc_character.o \
	mbaacc_framedata.o \
	mbaacc_cg.o \
	mbaacc_pack.o \
	texture.o \
	clone.o \
	render.o

CXX = g++ -m32
CFLAGS = -s -O3 -Os -Wall -I"$(CURDIR)" -I"$(CURDIR)/../libpng" -I"$(CURDIR)/../SDL" -I/mingw/i686-w64-mingw32/include/GL
LIBS = -static -lmingw32 -lSDLmain -lSDL -lpng -lz -lopengl32 -lcomctl32 -lole32 -lwinmm -ldxguid
LDFLAGS = -mwindows -L"$(CURDIR)/../libpng" -L"$(CURDIR)/../SDL"

all: test.exe

clean:
	rm -f *.o *.exe

.cpp.o:
	$(CXX) $(CFLAGS) -o $@ -c $<

test.exe: $(OBJS) test.o
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) test.o $(LIBS)
