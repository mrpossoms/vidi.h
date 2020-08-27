INC=-I../inc/
CFLAGS+=-g
LINK=-lavcodec -lavutil

glfw-cam: glfw-cam.c
	$(CC) $(CFLAGS) $(INC) $^ -o $@ $(LINK) -lglfw -lGL -lX11 -lXi -lXrandr -lXxf86vm -lXinerama -lXcursor -lrt -lm

ascii-cam: ascii-cam.c
	$(CC) $(CFLAGS) $(INC) $^ -o $@ $(LINK)

.PHONY: clean
clean:
	rm -f glfw-cam ascii-cam
