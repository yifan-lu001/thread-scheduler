#include "interface.h"
#include "scheduler.h"

// Interface implementation
// Implement APIs here...

// Initialize the CPU scheduler
void init_scheduler(enum sch_type type, int thread_count)
{
    // Initialize global variables
    schedule_type = type;
    num_threads = thread_count;
    threads_remaining = thread_count;
    global_time = 0;
    io_end_time = 0;

    // Initialize all mutexes
    pthread_mutex_init(&worker_mutex, NULL);
    pthread_mutex_init(&process_mutex, NULL);
    pthread_mutex_init(&semaphore_mutex, NULL);

    // Initialize all queues
    init_priority_queue(&cpu_queue);
    init_priority_queue(&io_queue);
    init_priority_queue(&threads_waiting);

    // Initialize semaphores array such that the initial value is 0
    semaphores = malloc(sizeof(struct semaphore) * MAX_NUM_SEM);
    for (int i = 0; i < MAX_NUM_SEM; i++)
    {
        semaphores[i].S = 0;
        init_priority_queue(&semaphores[i].queue);
    }

    // Initialize condition variables
    pthread_cond_init(&semaphore_cond, NULL);
    pthread_cond_init(&all_active_cond, NULL);
    pthread_cond_init(&ready, NULL);

    // Initially all variables each thread has
    thread_wakeup_conds = malloc(sizeof(pthread_cond_t) * thread_count);
    thread_run_conds = malloc(sizeof(pthread_cond_t) * thread_count);
    cpu_arrival_times = malloc(sizeof(float) * thread_count);
    io_durations = malloc(sizeof(int) * thread_count);
    active = malloc(sizeof(bool) * thread_count);
    consecutive_run_time = malloc(sizeof(int) * thread_count);
    last_run_time = malloc(sizeof(int) * thread_count);
    current_level = malloc(sizeof(int) * thread_count);

    for (int i = 0; i < thread_count; i++)
    {
        cpu_arrival_times[i] = -1.0;
        io_durations[i] = 0;
        active[i] = false;
        consecutive_run_time[i] = 0;
        last_run_time[i] = -2;
        current_level[i] = 0;
        pthread_cond_init(&thread_wakeup_conds[i], NULL);
        pthread_cond_init(&thread_run_conds[i], NULL);
    }

    mlfq_queues = malloc(sizeof(struct priority_queue) * 5);
    time_quantum = malloc(sizeof(int) * 5);

    // Initialize MLFQ queues
    for (int i = 0; i < 5; i++)
    {
        init_priority_queue(&mlfq_queues[i]);
        time_quantum[i] = 5*(1+i);
    }
}

// A thread calls this function for CPU burst, with the remaining_time in this burst
int cpu_me(float current_time, int tid, int remaining_time)
{
    // Wait until it can be processed
    pthread_mutex_lock(&process_mutex);
    pthread_mutex_lock(&worker_mutex);
    pthread_mutex_unlock(&process_mutex);

    if (remaining_time == 0)
    {
        cpu_arrival_times[tid] = -1.0; // Reset arrival time
        active[tid] = false;

        // Reset MLFQ info
        last_run_time[tid] = -2;
        consecutive_run_time[tid] = 0;
        current_level[tid] = 0;

        // Return control
        pthread_mutex_unlock(&worker_mutex);
        return current_time;
    }

    // Wait until the thread has arrived according to global clock
    wait_until_turn(tid, current_time);

    // Update the arrival time for the thread if needed
    if (cpu_arrival_times[tid] == -1.0)
    {
        cpu_arrival_times[tid] = current_time;
    }

    // Schedule thread
    schedule(&cpu_queue, schedule_type, tid, cpu_arrival_times[tid], remaining_time);

    // Completed scheduling
    pthread_cond_signal(&ready);
    // Give back the mutex (so the ready thread can run) and wait for this thread to be called
    pthread_cond_wait(&thread_run_conds[tid], &worker_mutex);

    // Finish thread
    active[tid] = false;
    int time = global_time;

    pthread_cond_signal(&ready); // Done running
    pthread_mutex_unlock(&worker_mutex);

    return time;
}

int io_me(float current_time, int tid, int duration)
{
    // Wait until it can be processed
    pthread_mutex_lock(&process_mutex);

    pthread_mutex_lock(&worker_mutex);
    pthread_mutex_unlock(&process_mutex);

    io_durations[tid] = duration;

    // Finished processing

    // Wait until the thread has arrived according to global clock
    wait_until_turn(tid, current_time);

    // Schedule thread
    schedule(&io_queue, 0, tid, current_time, -1);

    // Completed scheduling
    pthread_cond_signal(&ready);
    // Give back the mutex (so the ready thread can run) and wait for this thread to be called
    pthread_cond_wait(&thread_run_conds[tid], &worker_mutex);

    // Finish thread
    active[tid] = false;
    int time = global_time;

    pthread_cond_signal(&ready);
    pthread_mutex_unlock(&worker_mutex);

    return time;
}

int P(float current_time, int tid, int sem_id)
{
    // Wait until it can be processed
    pthread_mutex_lock(&process_mutex);
    pthread_mutex_lock(&worker_mutex);
    pthread_mutex_unlock(&process_mutex);

    // Wait until the thread has arrived according to global clock
    wait_until_turn(tid, current_time);

    pthread_mutex_lock(&semaphore_mutex);
    // Maybe these should not happen here
    push(&semaphores[sem_id].queue, tid, tid, -1);
    semaphores[sem_id].S--;

    bool will_wait = semaphores[sem_id].S < 0;

    if (!will_wait)
    {
        active[tid] = false;
    }
    pthread_cond_signal(&ready);
    pthread_mutex_unlock(&worker_mutex);

    if (will_wait)
    {
        pthread_cond_wait(&thread_run_conds[tid], &semaphore_mutex);
    }
    pop(&semaphores[sem_id].queue);
    active[tid] = false;
    int time = global_time;
    pthread_mutex_unlock(&semaphore_mutex);
    pthread_cond_signal(&semaphore_cond);

    return time;
}

int V(float current_time, int tid, int sem_id)
{
    pthread_mutex_lock(&process_mutex);
    pthread_mutex_lock(&worker_mutex);
    pthread_mutex_unlock(&process_mutex);

    wait_until_turn(tid, current_time);
    active[tid] = false;

    // Signal/Pause need to be able to pass between
    pthread_mutex_lock(&semaphore_mutex);
    // Maybe this should not happen here
    pthread_mutex_unlock(&worker_mutex);

    semaphores[sem_id].S++;
    if (semaphores[sem_id].S <= 0)
    {
        pthread_cond_signal(&thread_run_conds[semaphores[sem_id].queue.head->tid]);
        pthread_cond_wait(&semaphore_cond, &semaphore_mutex);
    }
    pthread_cond_signal(&ready);
    pthread_mutex_unlock(&semaphore_mutex);
    return ceil(current_time);
}

void end_me(int tid)
{
    pthread_mutex_lock(&process_mutex);
    pthread_mutex_lock(&worker_mutex);
    pthread_mutex_unlock(&process_mutex);

    threads_remaining--;
    if (all_active())
    {
        pthread_t global_clock_thread;
        pthread_create(&global_clock_thread, NULL, &threadFunc, NULL);
    }
    pthread_cond_signal(&ready);
    pthread_mutex_unlock(&worker_mutex);
}
