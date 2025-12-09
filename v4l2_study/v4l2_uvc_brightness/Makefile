all: main.o
	gcc main.o -o v4l2_uvc_brightness -lpthread

main.o: main.c
	gcc -c main.c

clean:
	rm -f *.o
	rm -f v4l2_uvc_brightness
