#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<string.h>
#include<pthread.h>
#include<dirent.h>
#include<limits.h>
#include<sys/stat.h>



char targetdir[PATH_MAX];

char *
path_cat (char * path, char * file)
{
	char * new_path = (char*) calloc(sizeof(char),PATH_MAX);
	strcpy(new_path, path);
	strcat(new_path, "/");
	strcat(new_path, file);
	return new_path;
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

int
send_s (size_t len, char * data, int sock)
{
 	size_t s;
	while(len > 0 && (s = send(sock, data, len, 0)) > 0){
		data += s;
		len -= s;
	}
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
put (int conn)
{
	int * size = (int *) malloc(sizeof(int));
	recv_s(sizeof(int), (char*)size, conn);

	char * name = (char*)malloc(*size);
	recv_s(*size, name, conn);
	name[*size]=0x0;
	free(size);	


	int * fail = (int *) malloc(sizeof(int));
	recv_s(sizeof(int), (char*)fail, conn);
	if (*fail == 1){
		printf("%s : not a regular file or does not exist\n",name);
		exit(1);
	}
	free(fail);

	//만약에 이미 존재하는 파일이면..? 위험

	char * path = path_cat(targetdir, name);
	free(name);

	size_t s;
	char buf[1024];
	FILE * f = fopen(path, "wb");
	while ( (s = recv(conn, buf, 1024, 0)) > 0 ) {
		fwrite(buf, s, 1, f);
	}
	free(path);
	fclose(f);
}

void
get (int conn)
{
	int * size = (int *) malloc(sizeof(int));
	recv_s(sizeof(int), (char*)size, conn);

	char * name = (char*)malloc(*size);
	recv_s(*size, name, conn);
	name[*size]=0x0;
	free(size);

	int fail = 0;
	char * path = path_cat(targetdir, name);
	if(isReg(path) == 0){
		fail = 1;
		goto err_file;
	}	
	
	FILE * f = fopen(path, "rb");
	if (f == NULL){
		fail = 1;
		goto err_file;
	}
	send_s(sizeof(int),(char*)&fail, conn);
	

	char buf[1024];
	size_t len;
	while(feof(f) == 0){
		len = fread(buf,1,sizeof(buf),f);
		send_s(len, buf, conn);
	}
	shutdown(conn, SHUT_WR);
	
	fclose(f);
	free(name);
	free(path);
	return ;	



err_file:
	send_s(sizeof(int),(char*)&fail, conn);
	free(name);
	free(path);
	return ;

}


void
list (int conn)
{
	DIR *dp;
	struct dirent * ep;
	dp = opendir(targetdir);
	//what if open error?
	
	char * nl = "\n";
	for( ;ep = readdir(dp); ){

		if(strcmp(ep->d_name,".") == 0){
			continue;
		}
		if(strcmp(ep->d_name,"..") == 0){
			continue;
			}
		if((ep->d_type == DT_LNK)){
			continue;
		}
		if((ep->d_type == DT_DIR)){
			continue;
		}
		send_s(strlen(ep->d_name), ep->d_name, conn);
		send_s(strlen(nl), nl, conn);
	}
	shutdown(conn, SHUT_WR);
	
}

void *
child_thread(void * fd)
{
	int conn = *(int *)fd;
	
	int * option = (int*) malloc(sizeof(int));
	recv_s(sizeof(int), (void*)option, conn);

	if(*option == 1){
		list(conn);
	}	
	if(*option == 2){
		get(conn);
	}	
	if(*option == 3){
		put(conn);
	}
	close(conn);
		
}


void
get_option (int argc, char ** argv, int * port, char ** dname)
{
	if(argc != 5){
  		printf("Wrong number of input\n");
		exit(1);
	}

	int opt;
	while(( opt = getopt(argc, argv, "p:d:")) != -1) {
		switch(opt) {
			case 'p':
				*port = atoi(optarg);
				//check port number is OK
				break;
 			case 'd':
				*dname = optarg;
				if(access(*dname,F_OK) != 0 || access(*dname,R_OK) !=0){
					perror("Directory error");
					exit(2);
				}
				break;
			default :
				printf("Unknown option\n");
		}
	}
	if(*port == -1 || dname == 0x0){
		printf("Invallid input : need two option -p, -d\n");
		exit(1);
	}       
}

int
main (int argc, char ** argv)
{

	int port = -1;
	char * dname = 0x0;
	get_option(argc,argv,&port,&dname);
	
	strcpy(targetdir, dname);	

	int listen_fd;
	struct sockaddr_in address;
	int addrlen = sizeof(address);

	listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(listen_fd == 0){
		perror("socket failed :");
		exit(EXIT_FAILURE);
	}
	
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);

	if(bind(listen_fd, (struct sockaddr *)&address, sizeof(address)) < 0){
		perror("bind failed ");
		exit(3);
	}

	while(1){
		if(listen(listen_fd, 16) < 0){
			perror("listen failed ");
			exit(3);
		}
		int * new_socket = (int*)malloc(sizeof(int));
		* new_socket = accept(listen_fd, (struct sockaddr *) &address, (socklen_t*)&addrlen);
		if(new_socket < 0){
			perror("accept failed ");
			exit(3);
		}
		pthread_t thread_t;
		if(pthread_create(&thread_t, 0x0, child_thread, (void*)new_socket) < 0){
			perror("thread create error");
			exit(4);
		}
	}
}
		
