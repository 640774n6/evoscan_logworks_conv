all: main.o libcsv.o
	gcc -std=c99 *.o -o evoscan_logworks_conv

main.o: main.c
	gcc -std=c99 -c main.c

libcsv.o: libcsv.c
	gcc -std=c99 -c libcsv.c

clean:
	-rm *.o evoscan_logworks_conv
