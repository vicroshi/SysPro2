 //
// Created by vic on 10/05/2024.
//

#ifndef SYSPRO1_PROTOCOL_H
#define SYSPRO1_PROTOCOL_H
#include <sys/types.h>

#define SERVER_FIFO "/tmp/sdi1900096_server"
#define job_triplet_len 19 //4 strlen("job_") + 12 digits + 2 spaces + 1 \newline

typedef struct request_header{
    pid_t pid;
    int command_num; /* 1 = issueJob <message>,
                      * 2=setConcurrency <N>,
                      * 3=stop job_<ID>,
                      * 4=poll <state>,
                      * 5=exit */
    int message; /*length of <message>
                      * or value of <N>
                      * or value of <ID>
                      * or value of <state> --->[1=running, 2=queued]*/
    int arg_count; // number of arguments in <message> or 0
} request_header;

// typedef struct jobTwople{ //a 2-tuple to hold the integer values of a job triplet
//     int job_id;
//     int queue_pos;
// } jobTwople;

typedef struct response_header{

    // int int1; //queuePos or number of jobs or -1
    // int len; //length of all descriptions or one job description or message or -1
    // int id; /* if id=-1,then
    //                 * if int1=-1 and len=-1, then no printing, //this is case of setConcurrency
    //                 * else if int1=-1, then print message where len its length //this is case of stop or exit
    //                 * else print poll where int1 = number_of_jobs, len = length of all descriptions //this is case of poll
    //         * else print issueJob triplet where int1 = queuePos and len = description length //this is case of issueJob
    //         */
} response_header;

#endif //SYSPRO1_PROTOCOL_H
