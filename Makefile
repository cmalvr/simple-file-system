CFLAGS = -c -g -ansi -pedantic -Wall -std=gnu99 `pkg-config fuse --cflags --libs`

LDFLAGS = `pkg-config fuse --cflags --libs`

# Uncomment on of the following three lines to compile
#SOURCES= sfs.c sfs_test0.c
#SOURCES= sfs.c sfs_test1.c
#SOURCES= sfs.c sfs_test2.c
SOURCES= sfs.c fuse_wrap_old.c
#SOURCES= sfs.c fuse_wrap_new.c

OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE= sfs

all: $(SOURCES) $(HEADERS) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	gcc $(OBJECTS) $(LDFLAGS) -o $@

.c.o:
	gcc $(CFLAGS) $< -o $@

clean:
	rm -rf *.o *~ $(EXECUTABLE)
