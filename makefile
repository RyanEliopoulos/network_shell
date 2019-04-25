server: mftpserv.o
	gcc -o server mftpserv.o
mftpserv.o: mftpserv.c
	gcc -c mftpserv.c
client: client.o
	gcc -o client client.o
client.o: client.c
	gcc -c client.c
