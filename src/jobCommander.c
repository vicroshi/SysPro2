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
#include <protocol.h>

char* join_strings(char**, const char*, int);

int main(int argc, char** argv){
    request_header rqh;
    rqh.arg_count = 0;
    char* command = NULL;
    if(strcmp(argv[3],"issueJob")==0){
        rqh.command_num = 1;
        command = join_strings(argv+4," ",argc-4);
        rqh.message = strlen(command);
        rqh.arg_count = argc-4;
    }
    else if(strcmp(argv[3],"setConcurrency")==0){
        rqh.command_num = 2;
        rqh.message = atoi(argv[2]);
    }
    else if(strcmp(argv[3],"stop")==0){
        rqh.command_num = 3;
        sscanf(argv[4],"job_%d",&rqh.message); //extract numeric value
    }
    else if(strcmp(argv[3],"poll")==0){
        rqh.command_num = 4;
        rqh.message = 0;
    }
    else if(strcmp(argv[3],"exit")==0){
        rqh.command_num = 5;
        rqh.message = 0;
    }
    else{
        exit(EXIT_FAILURE);
    }
    int sockfd;
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
    int nbytes = send(sockfd,&rqh, sizeof(rqh),0);
    printf("sent %d bytes\n",nbytes);
    if(rqh.command_num==1) {
        nbytes = send(sockfd, command, rqh.message, 0);
        printf("sent %d bytes\n",nbytes);
    }
    // if(nbytes<strlen(argv[3])){
        // perror("send");
    // }
    char* response_buffer = NULL;
    int response_length=0;
    recv(sockfd,&response_length,sizeof(int),0);
    printf("response length: %d\n",response_length);
    response_buffer = calloc(response_length,sizeof(char));
    recv(sockfd,response_buffer,response_length,0);
    printf("%s\n",response_buffer);
    if (rqh.command_num == 1) {
        int bytes_read = 0;
        while (bytes_read<sizeof(int)) {
            printf("about to recv\n");
            nbytes = recv(sockfd, (char*)&response_length+bytes_read, sizeof(int)-bytes_read, 0);
            if (nbytes == -1 ) {
                perror("recv");
                exit(EXIT_FAILURE);
            }
            printf("recv'd: %d bytes\n", nbytes);
            bytes_read += nbytes;
        }
        printf("response length: %d\n", response_length);
        free(response_buffer);
        response_buffer = malloc(response_length + 1);
        bytes_read = 0;
        while (bytes_read < response_length) {
            nbytes = recv(sockfd, response_buffer + bytes_read, response_length - bytes_read, 0);
            if (nbytes == -1) {
                perror("recv");
                exit(EXIT_FAILURE);
            }
            else if (nbytes == 0) {
                break;
            }
            bytes_read += nbytes;
        }
        printf("%s\n", response_buffer);
        free(command);
    }
    close(sockfd);
    return  0;
}

//literally made some last minute changes hoe it doesn't break anything :DDDDDDDDDD
char* join_strings(char** strings, const char* separator, int argc) { //used to join argv
    // calculate the total length of the resulting string
    size_t total_length = 0;
    for (int i = 0; i < argc; i++) {
        if (strchr(strings[i], ' ') != NULL) {
            total_length += 2; //for quotes
        }
        total_length += strlen(strings[i]) + 1;
    }
    char* result = malloc(total_length + 1);
    if (result == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    // copy the strings into the resulting string
    char* current_position = result;
    for (int i = 0; i < argc; i++) {
        if (strchr(strings[i], ' ') != NULL) { //if there is space in the string, add quotes
            strcpy(current_position, "\"");
            current_position++;
            strcpy(current_position, strings[i]);
            current_position += strlen(strings[i]);
            strcpy(current_position, "\"");
            current_position++;
        }
        else{ //else just copy
            strcpy(current_position, strings[i]);
            current_position += strlen(strings[i]);
        }
        //join them with separator
        strcpy(current_position, separator);
        current_position++;
    }
    // replace the last separator with a null terminator
    current_position--;
    *current_position = '\0';
    return result;
}