#define _XOPEN_SOURCE 500
#define _BSD_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <strings.h>
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

#define MAX_CLI 10
#define EVENTS_NO 15
#define MAX_MSG_SIZE 4000
#define CLI_NAME_SIZE 5



typedef struct client
{
  char name[CLI_NAME_SIZE];
  struct sockaddr addr;
  socklen_t len;
  int descr;
}client;

client clients[MAX_CLI];
int cli_num = 0;
struct epoll_event *events;
int myEpoll;
struct stat st;
int sockfd;
int file_handle;
int currindex;

void *service_func(void *);
int cli_accept(struct epoll_event );
void unregister_cli(const char*);
void register_cli(const char*, int);
void exit_handler();
void sigint_handler(int);

static int transfer_to_cli(const char *path, const struct stat *sb, int flag, struct FTW *ftwbuf){
	char msgbuff[MAX_MSG_SIZE];
	char tmppath[200];
	tmppath[0] = '\0';
	strcat(tmppath, path);
	char *pathptr = tmppath;
	while(pathptr[0]!='/')
		pathptr++;
	pathptr++;
	msgbuff[0] = '\0';
	if(S_ISDIR(sb->st_mode)){
		strcat(msgbuff, "d ");
		strcat(msgbuff, pathptr);
        if(send(events[currindex].data.fd, &msgbuff, MAX_MSG_SIZE, 0)<0){
            perror("send");
			return -1;
		}

	}
	else if(S_ISREG(sb->st_mode)){
		strcat(msgbuff, "r ");
		strcat(msgbuff, pathptr);
        if(send(events[currindex].data.fd, &msgbuff, MAX_MSG_SIZE, 0)<0){
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
				if((sent = send(events[currindex].data.fd, &msgbuff[off], MAX_MSG_SIZE-off, 0))<0){
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
	if(send(events[currindex].data.fd, &msgbuff, MAX_MSG_SIZE, 0)<0){ //???
        perror("send");
        return -1;
    }
	return 0;
}

int main(int argc, char *argv[])
{
    if(argc!= 2){
        printf("Sorry, wrong arguments\n");
        return -1;
    }
    char *serv_ipaddr = argv[1];
    int port_no = 4321;
    char buffer[256];
    struct sockaddr_in serv_addr, cli_addr;

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0))<0){
        perror("ERROR opening socket");
        return(-1);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port_no);
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if(bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))<0){
        perror("ERROR on binding");
        return -1;
    }
    listen(sockfd, 5);

    events = (struct epoll_event*)calloc(sizeof(struct epoll_event), EVENTS_NO);
    epoll_data_t data;
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    myEpoll = epoll_create1(0);
    data.fd = sockfd;
    event.data = data;
    epoll_ctl(myEpoll, EPOLL_CTL_ADD, sockfd, &event);

    atexit(exit_handler);
    signal(SIGINT, sigint_handler);

    pthread_t service_tid;
    pthread_create(&service_tid, NULL, &service_func, NULL);

    pthread_join(service_tid, NULL);
    return 0;
}

void *service_func(void *arg){
    char msgbuff[MAX_MSG_SIZE];
    char cname[10];
	int cli_sock;
	int ev_count;
    char command[10];
	char fulldname[50];
	char dirname[50];

    while(1){
        ev_count = epoll_wait(myEpoll, events, 1, -1);
		int i;
		for(i = 0; i<ev_count; i++){
			if(events[i].data.fd == sockfd){
				if((cli_sock = cli_accept(events[i]))<0){
					perror("cli_accept");
					exit(-1);
				}
 				if(recv(cli_sock, &msgbuff, MAX_MSG_SIZE, MSG_WAITALL) < 0){
					perror("Receive");
					exit(-1);
				}
				sscanf(msgbuff, "%s", cname);
				register_cli(cname, cli_sock);
				struct epoll_event event;

				event.data.fd = cli_sock;
				event.events = EPOLLIN | EPOLLET;
				if(epoll_ctl(myEpoll, EPOLL_CTL_ADD, cli_sock, &event)<0){
					perror("epoll_ctl");
					pthread_exit(0);
				}
			}
			else{
				int rec;
			  if((rec = recv(events[i].data.fd, &msgbuff, MAX_MSG_SIZE, MSG_WAITALL)) > 0){
				switch(msgbuff[0]){
					case 'i':
					    fulldname[0] = '\0';
					    sscanf(msgbuff,"%c %s %s",command, fulldname, dirname);
					    strcat(fulldname, "/");
					    strcat(fulldname, dirname);
                        if(mkdir(fulldname, 0700)<0){
							perror("mkdir");
						}
                        strcat(fulldname, "/.reco.txt");
                        if((file_handle = creat(fulldname, 0700))<0){
                            perror("creat");
                            pthread_exit((void*)-1);
                        }
                        msgbuff[0] = '\0';
                        strcat(msgbuff, "i ");
                        strcat(msgbuff, dirname);
                        if(send(events[i].data.fd, &msgbuff, MAX_MSG_SIZE, 0)<0){ 
                            perror("send");
                            pthread_exit((void*)-1);
                        }
						break;
					case 'p':
					    if(msgbuff[1] == 'l'){
							char comm[5];
							sscanf(msgbuff,"%s %s %s",comm, cname, dirname);
							char sourcePath[300];
							sourcePath[0] = '\0';
							strcat(sourcePath, cname);
							strcat(sourcePath, "/");
							strcat(sourcePath, dirname);
							DIR *dir;
							if((dir = opendir(sourcePath))==NULL){
								perror("opendir");
		                        msgbuff[0] = '\0';
		                        strcat(msgbuff, "NODIR");
		                        if(send(events[i].data.fd, &msgbuff, MAX_MSG_SIZE, 0)<0)
		                            perror("send");
								pthread_exit((void*) -1);
							}
							closedir(dir);
							currindex = i;
							if(nftw(sourcePath, transfer_to_cli, 10, 0)<0){
								perror("nftw");
								pthread_exit((void*)-1);
							}
							msgbuff[0] = '\0';
							strcat(msgbuff, "k");
							if(send(events[i].data.fd, &msgbuff, MAX_MSG_SIZE, 0)<0){ //???
                                perror("send");
                                pthread_exit((void*)-1);
                            }
						}
					    else if(msgbuff[1] == 's'){
							char comm[6];
					    	sscanf(msgbuff,"%s %s %s",comm, cname, dirname);
							int recvd;
							while((recvd = recv(events[i].data.fd, &msgbuff, MAX_MSG_SIZE, 0))>0){
								if(!strcmp(msgbuff, "k"))
									break;
								else if(msgbuff[0] == 'd'){
									char tmppath[200];
									char dirpath[200];
									char ftype;
									sscanf(msgbuff, "%c %s", &ftype, tmppath);
									dirpath[0] = '\0';
									strcat(dirpath, cname);
									strcat(dirpath, "/");
									strcat(dirpath, tmppath);
									if (stat(dirpath, &st) == -1) {
										mkdir(dirpath, 0700);
									}
								}
								else if(msgbuff[0] == 'r'){

									char tmppath[200];
									char filepath[200];
									char ftype;
									sscanf(msgbuff, "%c %s", &ftype, tmppath);
									filepath[0] = '\0';
									strcat(filepath, cname);
									strcat(filepath, "/");
									strcat(filepath, tmppath);
									FILE *file_handle;
									if(!(file_handle = fopen(filepath, "w"))){
										perror("fopen1");
										pthread_exit((void*)-1);
									}
			
									int recvd;
									do{
										if((recvd = recv(events[i].data.fd, &msgbuff, MAX_MSG_SIZE, 0))<0){
											perror("recv");
											fclose(file_handle);
											pthread_exit((void*)-1);
										}
										if(recvd == 0 || !strcmp(msgbuff, ""))
											break;
										int off = 0;
										do{
											int written;
											if((written = fwrite(&msgbuff[off], 1, strlen(msgbuff) - off, file_handle))<0){
												perror("fwrite");
												fclose(file_handle);
												pthread_exit((void*)-1);
											}
											off+=written;
										}while(off < strlen(msgbuff));
									}while(1);
									fclose(file_handle);
								}
							}					
						}
						break;
				}
			  }
			}
        }
		
    }
    pthread_exit(0);
}

void exit_handler(){
  shutdown(sockfd, SHUT_RDWR);
  close(sockfd);
  close(myEpoll);
  //unlink(sock_path);
}

void sigint_handler(int sig){
    exit(0);
}

void register_cli(const char* cname, int fd){
	char msgbuff[MAX_MSG_SIZE];
	int i;
	for(i = 0; i<cli_num; i++){
		if(strcmp(clients[i].name, cname)==0){
				sprintf(msgbuff,"%s","TAKEN");
				send(fd, &msgbuff, MAX_MSG_SIZE, MSG_NOSIGNAL);
				return;
		}
	}
    printf("New client %s registered\n",cname);
    sprintf(msgbuff,"Registered");
    strcpy(clients[cli_num].name, cname);
	cli_num++;
	cli_num%=MAX_CLI;
    clients[cli_num].descr = fd;
    if(stat(cname, &st) == -1) {
            mkdir(cname, 0700);
    }
    if(send(fd, &msgbuff, MAX_MSG_SIZE, 0)<0){
		perror("ERROR sending to client");
		exit(-1);
	}

}

int cli_accept(struct epoll_event event){
		struct sockaddr_storage addr;
		socklen_t size  = MAX_MSG_SIZE;
		int newsockfd;
		if((newsockfd = accept(sockfd, (struct sockaddr*)&addr, &size))<0){
            perror("ERROR accepting client connection");
            return -1;
        }
		return newsockfd;
}
