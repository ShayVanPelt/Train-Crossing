all: mts

mts: mts.c
	gcc mts.c -o mts -lpthread

clean:
	rm -f *.o
	rm -f mts
