resh: resh.o
	gcc -o resh resh.o

resh.o: resh.c
	gcc -o resh.o -c resh.c

clean:
	rm resh.o resh
