//
// Created by vic on 6/8/24.
//
// ReSharper disable CppLocalVariableMightNotBeInitialized
#define _GNU_SOURCE             /* See feature_test_macros(7) */

#include <errno.h>
#include <list.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>
#include <ring_buffer.h>
#include <fcntl.h>
#include <poll.h>
#include <protocol.h>
#include <sys/wait.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#define BACKLOG 20
#define ID_MAX_DIGITS 20
void* controllerThread(void* arg);
void* workerThread(void* arg);
char** tokenize(const char* input, int num_tokens);
typedef struct thread_args{
    int sockfd;
    ring_buffer* buffer;
} thread_args;
static pthread_cond_t cond_worker = PTHREAD_COND_INITIALIZER;
static pthread_cond_t cond_controller = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t mutex_cond = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mutex_freelist = PTHREAD_MUTEX_INITIALIZER;
int num_workers = 0;
int quit;
int main(int argc, char** argv){
    int sockfd, newsockfd;
    char* portnum = argv[1];
    int yes = 1;
    int bufferSize = atoi(argv[2]);
    int threadsNum = atoi(argv[3]);
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    int s = getaddrinfo(NULL,portnum,&hints,&res);
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
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }
        if(bind(sockfd, p->ai_addr,p->ai_addrlen)==-1){
            perror("bind");
            close(sockfd);
            continue;
        }
        break;
    }
    if(p == NULL){
        fprintf(stderr,"Could not bind\n");
        exit(EXIT_FAILURE);
    }
    freeaddrinfo(res);
    if(listen(sockfd,BACKLOG)==-1){
        perror("listen");
        exit(EXIT_FAILURE);
    }
    printf("Server is listening on port %s\n",portnum);
    struct sockaddr_storage client_addr;
    socklen_t addr_size = sizeof(client_addr);
    pthread_t w_threads[threadsNum];
    ring_buffer buffer;
    list c_threads;
    list_init(&c_threads);
    rbuffer_init(&buffer,bufferSize);
    thread_args args;
    args.buffer = &buffer;
    int exit_pipe[2];
    pipe2(exit_pipe,O_NONBLOCK);
    struct pollfd pollfds[2];
    pollfds[0].fd = sockfd;
    pollfds[0].events = POLLIN;
    pollfds[1].fd = exit_pipe[0];
    pollfds[1].events = POLLIN;
    for(int i=0;i<threadsNum;i++){
        pthread_create(&w_threads[i],NULL,workerThread,&args);
    }

    for (;;){
        while(poll(pollfds,2,-1)==-1){ //re-poll if EINTR occurs
            if(errno != EINTR){
                perror("poll:");
                exit(EXIT_FAILURE);
            }
        }
        if(pollfds[1].revents & POLLIN){
            char c;
            read(exit_pipe[0],&c,1);
            if(c=='q'){
                break;
            }
        }
        if(pollfds[0].revents & POLLIN) {
            newsockfd = accept(sockfd,(struct sockaddr*)&client_addr,&addr_size);
            if(newsockfd==-1){
                perror("accept");
                exit(EXIT_FAILURE);
            }
            args.sockfd = newsockfd;
            pthread_t controller;
            pthread_create(&controller,NULL,controllerThread,&args);
            pthread_detach(controller);
        }
    }
    for(int i=0;i<threadsNum;i++) {
        pthread_join(w_threads[0],NULL);
    }
    printf("All jobs are done\n");
    // close(sockfd);
    // close(newsockfd);
    return  0;
}

void* controllerThread(void* arg){
    thread_args* args = arg;

    pthread_t self = pthread_self();
    request_header rqh;
    int bytes_read = 0;
    while (bytes_read < sizeof(request_header)) {
        int nbytes = recv(args->sockfd, &rqh + bytes_read, sizeof(request_header)-bytes_read, 0);
        if (nbytes == -1) {
            perror("recv");
            return NULL;
        }
        bytes_read += nbytes;
    }
    // char* job_description = NULL;
    char* response_buffer = NULL;
    int n = 0; //response_buffer len;
    struct iovec* iov;
    struct msghdr msg;
    switch (rqh.command_num) {
        case 1:
            Job *job = malloc(sizeof(Job));
            job->pid = -1;
            bytes_read = 0;
            job->description = malloc(rqh.message+1);
            job->description[rqh.message] = '\0';
            while (bytes_read < rqh.message) {
                int nbytes = recv(args->sockfd, job->description + bytes_read, rqh.message - bytes_read, 0);
                if (nbytes == -1) {
                    perror("recv");
                    return NULL;
                }
                bytes_read += nbytes;
            }
            job->description_argc = rqh.arg_count;
            job->description_len  = rqh.message;
            pthread_mutex_lock(&mutex_cond);
            while (args->buffer->count >= args->buffer->size) {
                pthread_cond_wait(&cond_controller, &mutex_cond);
            }
            if (!quit) {
                rbuffer_enqueue(args->buffer, job);
                pthread_mutex_unlock(&mutex_cond);
                pthread_cond_signal(&cond_worker);
            }
            else {
                pthread_mutex_unlock(&mutex_cond);
            }
            break;
        case 2:
            pthread_mutex_lock(&mutex_cond);
            job = rbuffer_remove(args->buffer,rqh.message);
            pthread_mutex_unlock(&mutex_cond);
            response_buffer = malloc(strlen("JOB  NOTFOUND.")+ID_MAX_DIGITS+1); //20 gia MAX psifia kai 1 gia nullbyte
            if(job!=NULL){
                n = sprintf(response_buffer, "JOB %d REMOVED.", rqh.message) + 1;
            }
            else{
                n = sprintf(response_buffer, "JOB %d NOTFOUND.", rqh.message) + 1;
            }
            send(args->sockfd,response_buffer,n,0);
            free(response_buffer);
            break;
        case 3:
            pthread_mutex_lock(&mutex_cond);
            iov = malloc(args->buffer->count+1*sizeof(struct iovec));
            // iov[0].iov_base =
            int iov_idx = 1;
            int total_len = 0;
            for (int i = args->buffer->start; i != args->buffer->end; i = (++i == args->buffer->size ? 0 : i)){
                iov[iov_idx].iov_base = args->buffer->jobs[i]->description;
                iov[iov_idx].iov_len = args->buffer->jobs[i]->description_len;
                // iov[iov_idx].iov_base[iov[iov_idx].iov_len] = '\n';
                total_len += (args->buffer->jobs[i]->description_len + 1);
            }
            iov[0].iov_base = &total_len;
            iov[0].iov_len = sizeof(int);
            sendmsg(args->sockfd,&msg,0);
            pthread_mutex_lock(&mutex_cond);
            break;
        case 4:

            break;
        case 5:
            break;
        default:
            break;
    }
    // printf("Controller thread %d\n",*id);
    return NULL;
}

void* workerThread(void* arg){
    thread_args* args = arg;
    args->buffer;
    while(!quit){
        pthread_mutex_lock(&mutex_cond);
        while(args->buffer->count == 0 && num_workers == args->buffer->concurrency_level){
            pthread_cond_wait(&cond_worker,&mutex_cond);
            if(quit){
                return NULL;
            }
        }
        num_workers++;
        Job* job = rbuffer_dequeue(args->buffer);
        pthread_mutex_unlock(&mutex_cond);
        pthread_cond_signal(&cond_controller);
        char buffer[strlen(".output")+ID_MAX_DIGITS+1];
        buffer[0] = '\0';
        pid_t pid = fork();
        sprintf(buffer,"%d.output",job->id);
        if(pid == -1){
            perror("fork");
            exit(EXIT_FAILURE);
        }
        if(pid == 0) {
            char** job_args = tokenize(job->description, job->description_argc);
            int output_fd = open(buffer, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            dup2(STDOUT_FILENO,output_fd);
            close(STDOUT_FILENO);
            execvp(job_args[0],job_args);
            exit(EXIT_FAILURE);
        }
        waitpid(pid,NULL, 0);
        int output_fd = open(buffer, O_RDONLY);
        char bumper[strlen("-----job_ output start---") + ID_MAX_DIGITS + 1];
        bumper[sizeof(bumper)-1] = '\0';
        int n = sprintf(bumper, "----job_%d output start----", job->id);
        write(args->sockfd, bumper, n+1);
        struct stat st;
        lstat(buffer, &st);
        sendfile(args->sockfd, output_fd, 0, st.st_size);
        memset(bumper, 0, sizeof(bumper));
        sprintf(bumper, "----job_%d output end------", job->id);
        write(args->sockfd, bumper, n+1);
        free(job);
        // printf("Worker thread obtained job_%d\n",job->id);
    }
    return NULL;
}

char** tokenize(const char* input, int num_tokens) { //tokenize with space delimeter and keep quotes intact
    char* str = strdup(input);
    char** result = malloc((num_tokens + 1) * sizeof(char*)); // allocate memory for the known number of tokens
    int count = 0;
    char* token_start = str; //start of 1st token is the start of stirng
    int in_quotes = 0; //state for quotes
    //parse string
    for (char* p = str; *p; ++p) {
        if (*p == '\"' || *p == '\'') { //quotes beginning
            in_quotes = !in_quotes;
        }
        else if (*p == ' ' && !in_quotes) { //found a token, if we are not in quotes
            *p = '\0'; //the current token ends here
            result[count++] = strdup(token_start); //add the token to the list
            token_start = p + 1; //start new token
        }
    }

    // add the last token
    if (token_start != str + strlen(str)) {
        result[count++] = strdup(token_start);
    }
    result[num_tokens] = NULL;  // Null terminate the list of tokens
    //    printf("count: %d num_tokens: %d\n",count,num_tokens);
    //    for (int i = 0; i < count; i++) {
    //        printf("%s ",result[i]);
    //    }
    free(str);
    return result;
}