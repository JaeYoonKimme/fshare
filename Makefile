all: 
	gcc -o fshare fshare.c
	gcc -o fshared fshared.c -pthread

clean: 
	rm fshare
	rm fshared

