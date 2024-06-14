//
// Created by vic on 6/8/24.
//

#include "ring_buffer.h"
#include <stdlib.h>
#include <time.h>

void rbuffer_init(ring_buffer* buffer, int size) {
    buffer->jobs = malloc(size * sizeof(Job));
    buffer->size = size;
    buffer->start = 0;
    buffer->end = 0;
    buffer->count = 0;
}

Job* rbuffer_getJob(ring_buffer* buffer, int idx) {
    return buffer->jobs[(buffer->start + idx) % buffer->size];
}

Job* rbuffer_remove(ring_buffer* buffer, int id) {
    Job* job = NULL;
    int c = 0;
    for (int i = buffer->start; i != buffer->end;c++, i = (++i == buffer->size?0:i)) {
        if (buffer->jobs[i]->id == id) {
            job = buffer->jobs[i];
            c = i;
            break;
        }
    }
    int i = (c-1)%buffer->size;
    for (int j = c; j > buffer->start; j = (--j <= 0 ? buffer->size - 1 : j)) {
        buffer->jobs[j] = buffer->jobs[i];
        i = (--i < 0 ? buffer->size - 1 : i);
    }
    return job;
}

Job* rbuffer_dequeue(ring_buffer* buffer) {
    Job* job = buffer->jobs[buffer->start];
    buffer->start = (buffer->start + 1) % buffer->size;
    buffer->count--;
    return job;
}

Job* rbuffer_enqueue(ring_buffer* buffer, Job* job) {
    buffer->jobs[buffer->end] = job;
    buffer->end = (buffer->end + 1) % buffer->size;
    buffer->count++;
    return buffer->jobs[buffer->end - 1];
}

void rbuffer_destroy(ring_buffer* buffer) {
    for (int i = buffer->start; i != buffer->end; i = (i + 1) % buffer->size) {
        free(buffer->jobs[i]);
    }
    free(buffer->jobs);
}

