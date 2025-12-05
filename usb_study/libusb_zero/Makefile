
all: main.o
	gcc main.o -lusb-1.0 -o libusb_zero

main.o: main.c
	gcc -c main.c

clean:
	rm -f *.o
	rm -f libusb_zero
	 
