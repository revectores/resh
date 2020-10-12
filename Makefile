mysh: mysh.o
	gcc -o mysh mysh.o

mysh.o: mysh.c
	gcc -o mysh.o -c mysh.c

clean:
	rm mysh.o mysh
