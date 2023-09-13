#ifndef INTERFACE_H
#define INTERFACE_H

// Scheduler type
enum sch_type {
    SCH_FCFS = 0,   // first come first served
    SCH_SRTF = 1,   // shortest remaining time first
    SCH_MLFQ = 2,   // multi-level feedback queue
};
struct action_struct;

void init_scheduler(enum sch_type scheduler_type, int thread_count);

int cpu_me(float current_time, int tid, int remaining_time);
int io_me(float current_time, int tid, int duration);
int P(float current_time, int tid, int sem_id);
int V(float current_time, int tid, int sem_id);
void end_me(int tid);
void global_clock();
void * threadFunc(void * arg);

// Semaphore definitions
#define MAX_NUM_SEM 10  // sem_id from 0 to 9

#endif
