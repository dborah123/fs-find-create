CFLAGS=-g -Wall -pedantic
LDFLAGS=

.PHONY: all
all: fs-find

fs-find: fs-find.o
	$(CC) $(LDFLAGS) -o $(.TARGET) $(.ALLSRC)
.c:.o
	$(CC) $(CFLAGS) -c -o $(.TARGET) $(.IMPSRC)

clean: .PHONY
	rm -f *.o fs-find
