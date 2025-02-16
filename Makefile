BOFNAME := WebcamBOF
COMINCLUDE := -I .common
CC_x64 := x86_64-w64-mingw32-gcc
CC_x86 := i686-w64-mingw32-gcc


EXTRA_FLAGS := -fno-function-sections -fno-inline -fno-common -fno-data-sections -w

all:
	$(CC_x64) -o $(BOFNAME).x64.obj $(COMINCLUDE) -Os -fno-weak $(EXTRA_FLAGS) -c entry.c -DBOF 
	$(CC_x86) -o $(BOFNAME).x86.obj $(COMINCLUDE) -Os -fno-weak $(EXTRA_FLAGS) -c entry.c -DBOF 
	mkdir -p $(BOFNAME)
	mv $(BOFNAME)*.obj $(BOFNAME)

test:
	$(CC_x64) entry.c -g $(COMINCLUDE) $(LIBINCLUDE)  -o $(BOFNAME).x64.exe
	$(CC_x86) entry.c -g $(COMINCLUDE) $(LIBINCLUDE) -o $(BOFNAME).x86.exe
	mkdir -p $(BOFNAME)
	mv $(BOFNAME)*.exe $(BOFNAME)
	
scanbuild:
	$(CC) entry.c -o $(BOFNAME).scanbuild.exe $(COMINCLUDE) $(LIBINCLUDE)

check:
	ccheck --enable=all $(COMINCLUDE) --platform=win64 entry.c

clean:
	rm $(BOFNAME).*.exe
