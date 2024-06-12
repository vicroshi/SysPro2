//
// Created by vic on 6/8/24.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
int main(int argc, char** argv){
    int sockfd, newsockfd;
    char* hostname = argv[1];
    char* portno = argv[2];
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
//    hints.ai_protocol = 0;
    int s = getaddrinfo(hostname,portno,&hints,&res);
    if(s!=0){
        fprintf(stderr,"getaddrinfo: %s\n",gai_strerror(s));
        exit(EXIT_FAILURE);
    }
    struct addrinfo* p;
    for(p = res; p!=NULL; p=p->ai_next){
        if((sockfd = socket(p->ai_family,p->ai_socktype,p->ai_protocol))==-1){
            perror("socket");
            continue;
        }
        if(connect(sockfd,p->ai_addr,p->ai_addrlen)==-1){
            close(sockfd);
            perror("connect");
            continue;
        }
        break;
    }
    freeaddrinfo(res);
    if (p == NULL) {               /* No address succeeded */
        fprintf(stderr, "Could not connect\n");
        exit(EXIT_FAILURE);
    }
    int nbytes = send(sockfd,argv[3], strlen(argv[3]),0);
    if(nbytes<strlen(argv[3])){
        perror("send");
    }
    char buffer[256] = {0};
    recv(sockfd,buffer,256,0);
    printf("%s\n",buffer);
    close(sockfd);
    return  0;
}