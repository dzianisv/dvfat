CC_FLAGS=-g --std=c99

all: fusedfat.o libdfat.o list.o mkfs.dfat
	$(CC) $(CC_FLAGS) obj/fusedfat.o obj/libdfat.o obj/list.o -o out/fusedfat  -DFUSE_USE_VERSION=26 `pkg-config fuse --cflags --libs` 
	
fusedfat.o:
	$(CC) $(CC_FLAGS) fusedfat.c -lfuse -o obj/fusedfat.o -c  -DFUSE_USE_VERSION=26 `pkg-config fuse --cflags --libs`

libdfat.o: list.o
	$(CC) $(CC_FLAGS) libdfat.c  -c -o obj/libdfat.o

libdfat.so: libdfat.so
	$(CC) $(CC_FLAGS) obj/libdfat.o obj/list.o --shared out/libdfat.so

list.o: 
	$(CC) $(CC_FLAGS) -c list.c -o obj/list.o
 


mkfs.dfat: libdfat.o
	$(CC) $(CC_FLAGS) -c format.c -o obj/format.o
	$(CC) $(CC_FLAGS) obj/format.o obj/libdfat.o obj/list.o -o mkfs.dfat

test: libdfat.o
	$(CC) $(CC_FLAGS) test.c -c -o obj/test.o
	$(CC) $(CC_FLAGS) obj/test.o obj/libdfat.o obj/list.o -o test


clean:
	rm obj/*

