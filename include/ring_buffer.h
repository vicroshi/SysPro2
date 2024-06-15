//
// Created by vic on 6/8/24.
//

#ifndef SYSPRO2_RING_BUFFER_H
#define SYSPRO2_RING_BUFFER_H
#include <sys/types.h>
#include <stddef.h>
typedef struct Job {
    int id;
    int sockfd;
    char* description;
    int description_argc;
    int description_len;
//    QueueNode node;
}Job;

typedef struct ring_buffer {
    Job **jobs;
    int size;
    int start; //read
    int end;   //write
    //full: start+1 == end
    //empty: start == end
    int count;
} ring_buffer;
void rbuffer_init(ring_buffer*, int);
Job* rbuffer_getJob(ring_buffer*, int);
Job* rbuffer_remove(ring_buffer*, int);
Job* rbuffer_dequeue(ring_buffer*);
Job* rbuffer_enqueue(ring_buffer*, Job*);
void rbuffer_destroy(ring_buffer*);
#endif //SYSPRO2_RING_BUFFER_H
