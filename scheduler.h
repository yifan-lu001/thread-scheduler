#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <limits.h>
#include <pthread.h>

#include "interface.h"

// Global variables
int schedule_type;                     // The type of scheduler (0 = FCFS, 1 = SRTF, 2 = MLFQ)
struct priority_queue cpu_queue;       // Priority queue for CPU calls
struct priority_queue io_queue;        // Priority queue for I/O calls
struct priority_queue threads_waiting; // Priority queue for waiting threads
struct semaphore *semaphores;          // Array of semaphores
bool *active;                          // Array of active threads
bool io_active;
int global_time;                       // Global time variable
pthread_mutex_t worker_mutex;                 // mutex variable
pthread_mutex_t process_mutex;                 // mutex variable

pthread_mutex_t semaphore_mutex;                 // mutex variable

int num_threads;                       // The total number of threads
int threads_remaining;                 // The number of threads remaining

int io_end_time;                    
int *io_durations;

pthread_cond_t *thread_wakeup_conds;             // Array of pthread conds
pthread_cond_t *thread_run_conds;             // Array of pthread conds
pthread_cond_t ready;             // Array of pthread conds

pthread_cond_t semaphore_cond;         // Semaphore pthread cond
pthread_cond_t all_active_cond;        // All active pthread cond
float *cpu_arrival_times;              // Array of thread arrival times at the CPU

// consecutive run time array
int *consecutive_run_time;

// last run time array
int *last_run_time;

// Current level of each thread
int *current_level;

// Priority queue for each level
struct priority_queue *mlfq_queues;

// The time quantum for the 5 levels is 5, 10, 15, 20
int *time_quantum;


// Declare your own data structures and functions here...
// Priority queue of condition variables
struct priority_node {
    float priority1;
    float priority2;
    int tid;
    struct priority_node *next;
};

struct priority_queue {
    struct priority_node *head;
    pthread_mutex_t mutex;
};

// Semaphore struct
struct semaphore {
    int S;
    struct priority_queue queue;
};

void schedule_mlfq(struct priority_queue *queue, int tid, int arrival_time);
void update_mlfq_info(int tid);
void init_priority_queue(struct priority_queue *queue);
void push(struct priority_queue *queue, int tid, float priority1, float priority2);
int pop(struct priority_queue *queue);
int peek(struct priority_queue *queue);
void print_queue(struct priority_queue *queue);
void schedule(struct priority_queue *queue, int scheduler_type, int tid, float arrival_time, int remaining_time);
bool all_active();
bool signal_cpu();
bool signal_io();
void wait_until_turn(int tid, float time);
void * threadFunc(void * arg);
void global_clock();
#endif

