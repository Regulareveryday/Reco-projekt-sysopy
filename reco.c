#define _XOPEN_SOURCE 500
#define _BSD_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <signal.h>
#include <dirent.h>
#include <ftw.h>
#include <errno.h>

#define MAX_MSG_SIZE 4000

struct stat st;
int file_handle;
char msgbuff[MAX_MSG_SIZE];
int sockfd;
char *option;

void exit_handler();
void sigint_handler(int);
int connect_to_server(char*);
static int show_status(const char *path, const struct stat *sb, int flag, struct FTW *ftwbuf);
static int push(const char *path, const struct stat *sb, int flag, struct FTW *ftwbuf);

int main(int argc, char *argv[])
{   
	signal(SIGINT, sigint_handler);
	option = argv[1];
	if((strcmp(argv[1], "init")!=0) && (strcmp(argv[1], "pull")!=0) && (strcmp(argv[1], "status")!=0) && (strcmp(argv[1], "add")!=0) && (strcmp(argv[1], "push")!=0)){
		printf("Unrecognized command %s\n", argv[1]);
		return 0;
	}
	if((strcmp(argv[1], "init")==0) || (strcmp(argv[1], "pull")==0) || (strcmp(argv[1], "push")==0)){
		if(connect_to_server(argv[argc-1])<0)
				return -1;
	
		char *name =  argv[argc - 2];
		sprintf(msgbuff, "%s", name);
		if(send(sockfd, &msgbuff, MAX_MSG_SIZE, 0)<0){
		    perror("send");
		    return -1;
		}
		if(recv(sockfd, &msgbuff, MAX_MSG_SIZE, MSG_WAITALL)<0){
		    perror("Receive");
		    return -1;
		}
	}

	if(!strcmp(argv[1], "init")){
		char dirname[100];
		dirname[0] = '\0';
		strcat(dirname, argv[2]);
        if (stat(dirname, &st) == -1) {
            mkdir(dirname, 0700);
			char filename[100];
			filename[0] = '\0';
			strcat(filename, dirname);
            strcat(filename, "/.reco.txt");
            if((file_handle = creat(filename, 0700))<0){
                perror("creat");
                return -1;
            }
			filename[0] = '\0';
			strcat(filename, dirname);
            strcat(filename, "/.draft");
            mkdir(filename, 0700);
            msgbuff[0] = '\0';
            strcat(msgbuff, "i ");
			strcat(msgbuff, argv[3]);
			strcat(msgbuff, " ");
            strcat(msgbuff, argv[2]);
            if(send(sockfd, &msgbuff, MAX_MSG_SIZE, 0)<0){
                perror("send");
                return -1;
            }
				printf("Before\n");
            if(recv(sockfd, &msgbuff, MAX_MSG_SIZE, 0)<0){
                perror("Receive");
                return -1;
            }
			printf("After\n");
        }
	}
	else if(!strcmp(argv[1], "pull")){
		char *dirname = argv[2];
		msgbuff[0] = '\0';
		strcat(msgbuff, "pl ");
		strcat(msgbuff, argv[3]);
		strcat(msgbuff, " ");
		strcat(msgbuff, argv[2]);
		if(send(sockfd, &msgbuff, MAX_MSG_SIZE, 0)<0){
			perror("send");
			return -1;
		}
		
		int recvd;
		while((recvd = recv(sockfd, &msgbuff, MAX_MSG_SIZE, 0))>0){
			if(!strcmp(msgbuff, "k"))
				break;
			else if(msgbuff[0] == 'd'){
				char dirpath[200];
				char ftype;
				sscanf(msgbuff, "%c %s", &ftype, dirpath);
				if (stat(dirpath, &st) == -1) {
					mkdir(dirpath, 0700);
				}
				char draft[300];
				draft[0] = '\0';
				strcat(draft, argv[2]);
				strcat(draft, "/.draft/");
				char *pathptr = dirpath;
				while(pathptr[0]!='/')
					pathptr++;
				pathptr++;
				strcat(draft, pathptr);
				mkdir(draft, 0700);
			}
			else if(msgbuff[0] == 'r'){
				char filepath[200];
				char ftype;
				sscanf(msgbuff, "%c %s", &ftype, filepath);
				FILE *file_handle;
				if(!(file_handle = fopen(filepath, "w"))){
					perror("fopen1");
					return -1;
				}
				
				do{
					if((recvd = recv(sockfd, &msgbuff, MAX_MSG_SIZE, 0))<0){
						perror("recv");
						fclose(file_handle);
						return -1;
					}
					if(recvd == 0 || !strcmp(msgbuff, ""))
						break;
					int off = 0;
					do{
						int written;
						printf("%d\n", strlen(msgbuff));
						if((written = fwrite(&msgbuff[off], 1, strlen(msgbuff) - off, file_handle))<0){
							perror("fwrite");
							fclose(file_handle);
							return -1;
						}
						off+=written;
					}while(off < strlen(msgbuff));
				}while(1);
				fclose(file_handle);
				
				char draft[300];
				draft[0] = '\0';
				strcat(draft, argv[2]);
				strcat(draft, "/.draft/");
				char *pathptr = filepath;
				while(pathptr[0]!='/')
					pathptr++;
				pathptr++;
				strcat(draft, pathptr);
				
				if(!(file_handle = fopen(draft, "w"))){
					perror("fopen2");
					return -1;
				}
				fclose(file_handle);	
			}
		}					
	}
	else if(!strcmp(argv[1], "status")){

			DIR *dir;
			if(!(dir = opendir(argv[2]))){
				perror("opendir");
                return -1;
			}
			closedir(dir);
			if(nftw(argv[2], show_status, 10, 0)<0){
				perror("nftw");
				pthread_exit((void*)-1);
			}
	}
	else if(!strcmp(argv[1], "push")){

			DIR *dir;
			if(!(dir = opendir(argv[2]))){
				perror("opendir");
                return -1;
			}
			closedir(dir);
			msgbuff[0] = '\0';
            strcat(msgbuff, "ps ");
			strcat(msgbuff, argv[3]);
			strcat(msgbuff, " ");
            strcat(msgbuff, argv[2]);
            if(send(sockfd, &msgbuff, MAX_MSG_SIZE, 0)<0){
                perror("send");
                return -1;
            }
			if(nftw(argv[2], push, 10, 0)<0){
				perror("nftw");
				pthread_exit((void*)-1);
			}
			msgbuff[0] = '\0';
            strcat(msgbuff, "k ");
            if(send(sockfd, &msgbuff, MAX_MSG_SIZE, 0)<0){
                perror("send");
                return -1;
            }
	}
	else if(!strcmp(argv[1], "add")){
		struct stat info;
		if(lstat(argv[2], &info)!=0){
			if(errno == ENOENT){
				printf("Incorrect filepath\n");
				return -1;
			}
			else{
				perror("lstat");
				return -1;
			}
		}
		char *pathptr = argv[2];
		char draft[300];
		int i = 0;
		while(argv[2][i]!='/'){
			draft[i] = argv[2][i];
			pathptr++;
			i++;
		}
		if(pathptr[1] == '.')
			return 0;
		draft[i] = '\0';
		strcat(draft, "/.draft");
		strcat(draft, pathptr);
		FILE *file_handle;
		if(!(file_handle = fopen(draft, "w"))){
			perror("fopen");
			fclose(file_handle);
			return -1;
		}
		fclose(file_handle);
	}
    return 0;
}

static int push(const char *path, const struct stat *sb, int flag, struct FTW *ftwbuf){
	char msgbuff[MAX_MSG_SIZE];
	msgbuff[0] = '\0';
	if(S_ISDIR(sb->st_mode)){
		char tmppath[300];
		tmppath[0] = '\0';
		strcat(tmppath, path);
		char *pathptr = tmppath;
		int i = 0;
		while(tmppath[i]!='/'){
			pathptr ++;
			i++;
		}
		if(pathptr[1] == '.')
			return 0;
		strcat(msgbuff, "d ");
		strcat(msgbuff, path);
        if(send(sockfd, &msgbuff, MAX_MSG_SIZE, 0)<0){
            perror("send");
			return -1;
		}

	}
	else if(S_ISREG(sb->st_mode)){
		char tmppath[300];
		tmppath[0] = '\0';
		strcat(tmppath, path);
		char *pathptr = tmppath;
		int i = 0;
		while(tmppath[i]!='/'){
			pathptr ++;
			i++;
		}
		if(pathptr[1] == '.')
			return 0;
		strcat(msgbuff, "r ");
		strcat(msgbuff, path);
        if(send(sockfd, &msgbuff, MAX_MSG_SIZE, 0)<0){
            perror("send");
			return -1;
		}
		FILE *file_handle;
		if((file_handle = fopen(path, "r"))==NULL){
			perror("fopen");
			return -1;
		}
		while(!feof(file_handle)){
			memset(msgbuff, '\0', MAX_MSG_SIZE);
			int b_read;
			if((b_read = fread(msgbuff, 1, MAX_MSG_SIZE, file_handle))<0){//????
				perror("fread");
				fclose(file_handle);
				return -1;
			}
			
			int off = 0;
			do{
				int sent;
				if((sent = send(sockfd, &msgbuff[off], MAX_MSG_SIZE-off, 0))<0){
					perror("send");
					fclose(file_handle);
					return -1;
				}
				off += sent;
			}while(off < b_read);
		}
		fclose(file_handle);	
	}
	msgbuff[0] = '\0';
	if(send(sockfd, &msgbuff, MAX_MSG_SIZE, 0)<0){ 
        perror("send");
        return -1;
    }
	return 0;
}

static int show_status(const char *path, const struct stat *sb, int flag, struct FTW *ftwbuf){
	if(!S_ISREG(sb->st_mode))
		return 0;
	char tmppath[300];
	tmppath[0] = '\0';
	strcat(tmppath, path);
	char *pathptr = tmppath;
	char draft[300];
	int i = 0;
	while(tmppath[i]!='/'){
		draft[i] = tmppath[i];
		pathptr++;
		i++;
	}
	if(pathptr[1] == '.')
		return 0;
	draft[i] = '\0';
	strcat(draft, "/.draft");
	strcat(draft, pathptr);
	struct stat info1, info2;
	if(lstat(draft, &info1)!=0){
		if(errno == ENOENT){
			printf("%s not up to date\n", path);
		}
		else{
			perror("lstat");
			return -1;
		}
	}
	else{
		if(lstat(path, &info2)!=0){
			perror("lstat");
			return -1;
		}
		if(info1.st_mtime < info2.st_mtime)
			printf("%s not up to date\n", path);
	}
	return 0;
}

void exit_handler(){
	if((strcmp(option, "init")==0) || (strcmp(option, "pull")==0) || (strcmp(option, "push")==0)){
		shutdown(sockfd, SHUT_RDWR);
		close(sockfd);
	}
}

void sigint_handler(int sig){
    exit(0);
}

int connect_to_server(char *serv_addr){
	struct sockaddr_in addr;
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0))<0){
        perror("ERROR creating socket");
        return -1;
    }
    addr.sin_family=AF_INET;
    addr.sin_port=htons(4321);
    inet_pton(AF_INET, serv_addr, &(addr.sin_addr));
    if((connect(sockfd,(struct sockaddr*)&addr, sizeof(struct sockaddr_in)))!=0){
        perror("ERROR connecting to server");
        return -1;
    }
	atexit(exit_handler);
	return 0;
}
