//
// Created by vic on 6/8/24.
//
// ReSharper disable CppLocalVariableMightNotBeInitialized
#define _GNU_SOURCE             /* See feature_test_macros(7) */

#include <errno.h>
#include <sys/mman.h>
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
    int exit_pipe;
} thread_args;


static pthread_cond_t cond_worker = PTHREAD_COND_INITIALIZER;
static pthread_cond_t cond_controller = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t mutex_cond = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mutex_concurrency = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mutex_jid = PTHREAD_MUTEX_INITIALIZER;
int concurrency_level=1;
int job_id = 1;
int num_workers = 0;
int quit = 0;
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
    args.exit_pipe = exit_pipe[1];
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
                quit =1;
                pthread_cond_broadcast(&cond_worker);
                pthread_cond_broadcast(&cond_controller);
                for(int i=0;i<threadsNum;i++){
                    pthread_join(w_threads[i],NULL);
                }
                break;
            }
        }
        if(pollfds[0].revents & POLLIN) {
            newsockfd = accept(sockfd,(struct sockaddr*)&client_addr,&addr_size);
            if(newsockfd==-1){
                perror("accept");
                exit(EXIT_FAILURE);
            }
            /*request_header rqh;
            recv(newsockfd,&rqh,sizeof(request_header),0);
            if (rqh.command_num == 5) {
                int nbytes = strlen("SERVER TERMINATED");
                send(newsockfd, &nbytes, sizeof(int), 0);
                send(newsockfd, "SERVER TERMINATED", nbytes, 0);
                break;
            }
            else if (rqh.command_num == 1) {
                char buffer[rqh.message+1];
                buffer[rqh.message] = '\0';
                // memset(buffer, 0, rqh.message);
                recv(newsockfd, buffer, rqh.message, 0);
                int nbytes = strlen("JOB <job_,> SUBMITTED")+rqh.message+1+ID_MAX_DIGITS+1;
                char response_buffer[nbytes];
                memset(response_buffer, 0, nbytes);
                int n = sprintf(response_buffer, "JOB <job_%d,%s> SUBMITTED", job_id++, buffer);
                send(newsockfd, &n, sizeof(int), 0);
                send(newsockfd, response_buffer, n, 0);
                send(newsockfd, &rqh.message, sizeof(int), 0);
                send(newsockfd, buffer, rqh.message, 0);
            }*/
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
    close(sockfd);
    close(newsockfd);
    return  0;
}

void* controllerThread(void* arg){
    thread_args* args = arg;

    request_header rqh;
    int bytes_read = 0;
    while (bytes_read < sizeof(request_header)) {
        int nbytes = recv(args->sockfd, (char*)&rqh + bytes_read, sizeof(request_header)-bytes_read, 0);
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
            pthread_mutex_lock(&mutex_jid);
            job->id = job_id++;
            pthread_mutex_unlock(&mutex_jid);
            bytes_read = 0;
            job->sockfd = args->sockfd;
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
            response_buffer = calloc(strlen("JOB <job_,> SUBMITTED")+ID_MAX_DIGITS+rqh.message+1,sizeof(char)); //20 gia MAX psifia kai 1 gia nullbyte
            n = sprintf(response_buffer, "JOB <job_%d,%s> SUBMITTED", job->id,job->description) + 1;
            pthread_mutex_lock(&mutex_cond);
            while (args->buffer->count >= args->buffer->size) {
                pthread_cond_wait(&cond_controller, &mutex_cond);
            }
            if (!quit) {
                rbuffer_enqueue(args->buffer, job);
                pthread_mutex_unlock(&mutex_cond);
                pthread_cond_signal(&cond_worker);
                send(args->sockfd, &n, sizeof(int), 0);
                send(args->sockfd, response_buffer, n, 0);
                //need to send job submitted message
                // send(args->sockfd, &job->id, sizeof(int), 0);
            }
            else {
                pthread_mutex_unlock(&mutex_cond);
            }
            break;
        case 2:
            pthread_mutex_lock(&mutex_concurrency);
            concurrency_level = rqh.message;
            pthread_mutex_unlock(&mutex_concurrency);
            pthread_cond_broadcast(&cond_worker);
            break;
        case 3:
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
        case 4:
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
        case 5:
            pthread_mutex_lock(&mutex_cond);
            write(args->exit_pipe,"q",1);
            for (int i = args->buffer->start; i != args->buffer->end; i = (++i == args->buffer->size ? 0 : i)){
                free(args->buffer->jobs[i]->description);
                free(args->buffer->jobs[i]);
                free(args->buffer->jobs);
            }
            pthread_mutex_unlock(&mutex_cond);
            break;
        default:
            break;
    }
    free(response_buffer);
    free(iov);
    // printf("Controller thread %d\n",*id);
    return NULL;
}

void* workerThread(void* arg){
    thread_args* args = arg;
    ring_buffer* rbuffer = args->buffer;
    int sockfd;
    while(!quit){
        pthread_mutex_lock(&mutex_cond);
        while(rbuffer->count == 0 || num_workers == concurrency_level){
            pthread_cond_wait(&cond_worker,&mutex_cond);
            if(quit){
                return NULL;
            }
        }
        num_workers++;
        Job* job = rbuffer_dequeue(rbuffer);
        pthread_mutex_unlock(&mutex_cond);
        pthread_cond_signal(&cond_controller);
        printf("Worker thread obtained job_%d\n",job->id);
        char buffer[strlen(".output")+ID_MAX_DIGITS+1];
        buffer[sizeof buffer - 1] = '\0';
        pid_t pid = fork();
        if(pid == -1){
            perror("fork");
            exit(EXIT_FAILURE);
        }
        if(pid == 0) {
            sprintf(buffer,"%d.output",getpid());
            char** job_args = tokenize(job->description, job->description_argc);
            // for(int i=0;i<job->description_argc;i++){
                // printf("%s ",job_args[i]);
            // }
            int output_fd = open(buffer, O_WRONLY | O_CREAT, 0666);
            if(output_fd == -1){
                perror("open wr");
                exit(EXIT_FAILURE);
            }
            // printf("opened %s\n",buffer);
            dup2(output_fd,STDOUT_FILENO);
            close(output_fd);
            execvp(job_args[0],job_args);
            printf("execvp failed\n");
            exit(EXIT_FAILURE);
        }
        waitpid(pid,NULL, 0);
        sprintf(buffer,"%d.output",pid);
        int output_fd = open(buffer, O_RDONLY);
        if(output_fd == -1){
            fprintf(stderr,"can't open %s\n",buffer);
            exit(EXIT_FAILURE);
        }
        int bumper_len = strlen("\n-----job_ output start---\n") + ID_MAX_DIGITS + 1;
        struct stat st;
        if(lstat(buffer, &st)==-1){
            perror("lstat");
            exit(EXIT_FAILURE);
        }
        char bumper1[bumper_len];
        char bumper2[bumper_len];
        bumper1[sizeof(bumper1)] = '\0';
        bumper2[sizeof(bumper2)] = '\0';
        int n1 = sprintf(bumper1, "\n----job_%d output start----\n", job->id);
        int n2 = sprintf(bumper2, "\n-----job_%d output end-----\n", job->id);
        int n = n2+n1+st.st_size+1;
        // printf("%d+%d+%d+1\n",n2,n1,st.st_size);
        // printf("%d+%d+%d+1\n",strlen(bumper2),strlen(bumper1),st.st_size);
        // printf("%d+%d+%d+1\n",bumper_len,bumper_len,st.st_size);
        sockfd = job->sockfd;
        printf("n: %d\n",n);
        if(send(job->sockfd, &n,sizeof(int),0)==-1) {
            perror("send");
            exit(EXIT_FAILURE);
        }
        printf("sent %d\n",n);
        send(sockfd, bumper1, n1,0);
        /*char outputbuffer[st.st_size];
        memset(outputbuffer,0,st.st_size);
        if(read(output_fd,outputbuffer,st.st_size)==-1) {
            perror("read");
            fprintf(stderr, "errno:%s\n", strerrorname_np(errno));
        }
        if(send(sockfd,outputbuffer,st.st_size,0)==-1) {
            perror("send");
            fprintf(stderr, "errno:%s\n", strerrorname_np(errno));
        }*/

        int bytes_sent = 0;
        off_t offset = 0;
        while (bytes_sent < st.st_size) {
            int nbytes = sendfile(sockfd, output_fd, &offset, st.st_size-bytes_sent);
            if (nbytes == -1) {
                perror("sendfile");
                fprintf(stderr, "errno:%s\n", strerrorname_np(errno));
                exit(EXIT_FAILURE);
            }
            bytes_sent += nbytes;
        }
        send(sockfd, bumper2, n2+1,0);
        free(job);
        close(output_fd);
        unlink(buffer);
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