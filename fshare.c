#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<string.h>
#include<limits.h>
#include<sys/stat.h>


int
get_ip_port(char * addr, char ** ip, int * port) 
{
	char * ptr;
	char * n_ptr;

	ptr = strtok_r(addr, ":", &n_ptr);
	if(ptr == 0x0){
		return -1;
	}
	*ip = strdup(ptr);
	ptr = strtok_r(NULL, "", &n_ptr);	
	*port = atoi(ptr);

}

int
isReg (char * path)
{
	struct stat st;
	if(stat(path,&st) == -1){
		perror("stat error\n");
		exit(7);
	}

	if(S_ISREG(st.st_mode)){
		return 1;
	}
	else{
		return 0;
	}
}

void
get_option (int argc, char ** argv, char ** ip, int * port, char ** fname, int * option)
{
	if(!(argc == 3 || argc == 4)){
		goto input_err;
	}
	if(get_ip_port(argv[1],ip,port) == -1){
		printf("Wrong format address\n");
		exit(1);
	}

	if(strcmp(argv[2],"list") == 0){
		if(argc != 3){
			goto input_err;
		}
		*option = 1;
	}
	else if(strcmp(argv[2],"get") == 0){
		if(argc != 4){
			goto input_err;
		}
		if(strstr(argv[3],"/")!= 0x0){
			printf("wrong file name : give a file name, not a path\n");
			exit(2);
		}
		*option = 2;
		*fname = argv[3];
	}
	else if(strcmp(argv[2],"put") == 0){
		if(argc != 4){
			goto input_err;
		}
		if(strstr(argv[3],"/")!= 0x0){
			printf("wrong file name : give a file name, not a path\n");
			exit(2);
		}
		if(isReg(argv[3]) == 0){
			printf("%s : not a regular file or does not exist\n",argv[3]);
			exit(2);
		}
		*option = 3;
		*fname = argv[3];
	}
	else{
		perror("Unkown option");
		exit(1);
	}

	return;

input_err:
	printf("Error : wrong input format\n");
	exit(1);

}

void 
send_s (size_t len, char * data, int sock)
{
	size_t s;
	while(len > 0 && (s = send(sock, data, len, 0)) > 0){
		data += s;
		len -= s;
	}
}

void
recv_s (size_t len, char * data, int sock)
{
	size_t s;
	while(len > 0 && (s = recv(sock, data, len, 0)) > 0){
		data += s;
		len -= s;
	}
}

char *
recv_data (char * data, int sock){
	int len = 0;
        int s;
        char buf[1024];
        while ( (s = recv(sock, buf, 1023, 0)) > 0 ) {
                buf[s] = 0x0 ;
                if (data == 0x0) {
                        data = strdup(buf) ;
                        len = s ;
                }
                else {
                        data = realloc(data, len + s + 1) ;
                        strncpy(data + len, buf, s) ;
                        data[len + s] = 0x0 ;
                        len += s ;
                }

        }
	return data;
}

void
list (int conn)
{
	char * data = 0x0;
	data = recv_data(data, conn);
	printf("%s\n",data);
	free(data);
}

void
get (int conn, char * fname)
{	
	int size = strlen(fname);
	send_s(sizeof(int), (char*)&size, conn);
	send_s(size, fname, conn);

	shutdown(conn, SHUT_WR);
	int * fail = (int *) malloc(sizeof(int));
	recv_s(sizeof(int), (char*)fail, conn);
	
	if (*fail == 1){
		printf("%s : not a regular file or does not exist\n",fname);
		exit(1);
	}

	size_t s;
	char buf[1024];
	FILE * f = fopen(fname, "wb");
	while ( (s = recv(conn, buf, 1024, 0)) > 0 ) {
		fwrite(buf, s, 1, f);
	}
	fclose(f);
	free(fail);
	
}

void
put (int conn, char * fname)
{
	int size = strlen(fname);
	send_s(sizeof(int), (char*)&size, conn);
	send_s(size, fname, conn);
	

	int fail = 0;
	FILE * f = fopen(fname, "rb");
	if(f == NULL){
		fail = 1;
		goto err_file;
	}
	send_s(sizeof(int), (char*)&fail, conn);


	char buf[1024];
        size_t len;
        while(feof(f) == 0){
                len = fread(buf,1,sizeof(buf),f);
                send_s(len, buf, conn);
        }
        shutdown(conn, SHUT_WR);
	return ;



err_file:
	send_s(sizeof(int), (char*)&fail, conn);
	return ;
}


int
main (int argc, char ** argv)
{

	char * ip = 0x0;
	int port = -1;
	char * fname = 0x0;
	int  option;
	get_option(argc,argv,&ip,&port,&fname,&option);

	int sock_fd;
	struct sockaddr_in address;
	int addrlen = sizeof(address);

	sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(sock_fd == 0){
		perror("socket failed :");
		exit(EXIT_FAILURE);
	}
	
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_port = htons(port);

	if (inet_pton(AF_INET, ip, &address.sin_addr) <= 0) {
		perror("inet_pton failed : ") ;
		exit(EXIT_FAILURE) ;
	} 
	if(connect(sock_fd, (struct sockaddr *)&address, sizeof(address)) < 0){
		perror("connect failed ");
		exit(3);
	}


	//Send mode
	send_s(sizeof(int), (char*)&option, sock_fd);

	if(option == 1){
		shutdown(sock_fd, SHUT_WR);
		list(sock_fd);
	}
	if(option == 2){
		get(sock_fd, fname);
	}

	if(option == 3){
		put(sock_fd, fname);
	}



}
