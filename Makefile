CFLAGS=-g -Wall -pedantic
LDFLAGS=

.PHONY: all

fs-find: fs-find.o
	$(CC) $(LDFLAGS) -o $(.TARGET) $(.ALLSRC)
.c:.o
	$(CC) $(CFLAGS) -o $(.TARGET) $(.IMPSRC)

clean: .PHONY
	rm -f *.o fs-find
