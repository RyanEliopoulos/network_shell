server: mftpserv.o
	gcc -o server mftpserv.o
mftpserv.o: mftpserv.c
	gcc -c mftpserv.c
client: daytime.o
	gcc -o client daytime.o
daytim.o: daytime.c
	gcc -c daytime.c
