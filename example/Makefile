INC=-I../inc/
CFLAGS+=-g
LINK=

ascii-cam: ascii-cam.c
	$(CC) $(CFLAGS) $(INC) $^ -o $@ $(LINK)

.PHONY: clean
clean:
	rm ascii-cam
