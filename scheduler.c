#include "scheduler.h"

// Scheduler implementation
// Implement all other functions here...

// initialize a priority queue
void init_priority_queue(struct priority_queue *queue)
{
    queue->head = NULL;
    pthread_mutex_init(&queue->mutex, NULL);
}

// push to priority queue
void push(struct priority_queue *queue, int tid, float priority1, float priority2)
{
    pthread_mutex_lock(&queue->mutex);
    struct priority_node *new_node = (struct priority_node *)malloc(sizeof(struct priority_node));
    new_node->tid = tid;
    new_node->priority1 = priority1;
    new_node->priority2 = priority2;
    new_node->next = NULL;

    if (queue->head == NULL)
    {
        queue->head = new_node;
    }
    else
    {
        struct priority_node *current = queue->head;
        struct priority_node *prev = NULL;
        while (current != NULL)
        {
            if (current->priority1 > priority1)
            {
                break;
            }
            else if (current->priority1 == priority1)
            {
                if (current->priority2 > priority2)
                {
                    break;
                }
            }
            prev = current;
            current = current->next;
        }
        if (prev == NULL)
        {
            new_node->next = queue->head;
            queue->head = new_node;
        }
        else
        {
            new_node->next = current;
            prev->next = new_node;
        }
    }
    pthread_mutex_unlock(&queue->mutex);
}

// pop from priority queue
int pop(struct priority_queue *queue)
{
    pthread_mutex_t *mutex = &queue->mutex;
    pthread_mutex_lock(mutex);
    struct priority_node *node = queue->head;
    if (node == NULL)
    {
        pthread_mutex_unlock(mutex);
        return -1;
    }
    queue->head = node->next;
    pthread_mutex_unlock(mutex);
    int tid = node->tid;
    free(node);
    return tid;
}

int peek(struct priority_queue *queue)
{
    pthread_mutex_t *mutex = &queue->mutex;
    pthread_mutex_lock(mutex);
    struct priority_node *node = queue->head;
    if (node == NULL)
    {
        pthread_mutex_unlock(mutex);
        return -1;
    }
    pthread_mutex_unlock(mutex);
    return node->tid;
}

// Add a thread to the MLFQ
void schedule_mlfq(struct priority_queue *queue, int tid, int arrival_time)
{
    update_mlfq_info(tid);

    int level = current_level[tid];
    if (level < 4) // As long as it isn't already last
    {
        // If the thread has run for the time quantum, increase its level
        if (consecutive_run_time[tid] + 1 >= time_quantum[level])
        {
            current_level[tid]++;
            // Reset consecutive run time if level is increased
            consecutive_run_time[tid] = 0;
        }
    }

    // Add the thread to the queue
    push(&mlfq_queues[level], tid, arrival_time, tid);
}

// Update consecutive run time and last run time
void update_mlfq_info(int tid)
{       
    // Update consecutive run time
    if (last_run_time[tid] == global_time)
    {
        consecutive_run_time[tid]++;
    }
    else
    {
        consecutive_run_time[tid] = 0;
    }
}

void schedule(struct priority_queue *queue, int scheduler_type, int tid, float arrival_time, int remaining_time)
{
    float priority1 = 0.0;
    float priority2 = 0.0;
    switch (scheduler_type)
    {
    case 0: // FCFS
        priority1 = arrival_time;
        priority2 = tid;
        break;
    case 1: // SRTF
        priority1 = remaining_time;
        priority2 = tid;
        break;
    case 2: // MLFQ
        schedule_mlfq(queue, tid, arrival_time);
        return;
    }
    push(queue, tid, priority1, priority2);
}

bool all_active()
{
    // Count the number of active threads
    int count = 0;
    for (int i = 0; i < num_threads; i++)
    {
        if (active[i])
        {
            count++;
        }
    }
    return count == threads_remaining;
}

// If there's at least one thread in cpu_queue, signal the cpu
bool signal_cpu()
{
    int tid_to_run = -1;

    // If it's MLFQ, check the queues from level 0 to 3
    if (schedule_type == 2)
    {
        for (int i = 0; i < 4; i++)
        {
            tid_to_run = peek(&mlfq_queues[i]);
            if (tid_to_run != -1)
            {
                // Update last run time
                last_run_time[tid_to_run] = global_time;

                // Pop it
                pop(&mlfq_queues[i]);
                break;
            }
        }
    }
    else
    {
        tid_to_run = peek(&cpu_queue);
        if (tid_to_run != -1)
        {
            // Pop it
            pop(&cpu_queue);
        }
    }    

    // If there's at least one thread in cpu_queue, signal the cpu
    if (tid_to_run != -1)
    {
        pthread_cond_signal(&thread_run_conds[tid_to_run]);
        pthread_cond_wait(&ready, &worker_mutex);
        return 1;
    }
    return 0;
}

bool signal_io()
{
    if (io_queue.head != NULL)
    {
        // Calculate when the top thread should finish
        // Top thread is always the correct one to run since it is FCFS
        int tid = peek(&io_queue);
        float top_priority = io_queue.head->priority1;
        int end_time = fmax(io_end_time, top_priority) + io_durations[tid];

        // If it is time to run, signal the thread to finish
        if (end_time <= global_time)
        {
            io_end_time = end_time;

            pthread_cond_signal(&thread_run_conds[pop(&io_queue)]);
            pthread_cond_wait(&ready, &worker_mutex);
            return 1;
        }
    }
    return 0;
}

// Has thread wait on condition variable that will be triggered by global_clock
void wait_until_turn(int tid, float time)
{
    // Set the thread to be active
    active[tid] = true;

    // Add this as a waiting thread
    push(&threads_waiting, tid, time, 0.0);

    // If all threads are active, run the global clock
    if (all_active())
    {
        /*
        Need global clock to run simultaneously to the current thread's action
        so we create a new thread that will run global_clock.
        */
        pthread_t global_clock_thread;
        pthread_create(&global_clock_thread, NULL, &threadFunc, NULL);
    }

    // Wait for this thread to be called
    pthread_cond_wait(&thread_wakeup_conds[tid], &worker_mutex);
}

// Thread initialization function that simply calls the global_clock
void *threadFunc(void *arg)
{
    global_clock();
}

// Main function loops global time and calls the threads
void global_clock()
{
    pthread_mutex_lock(&process_mutex); // Lock threads out of being processed
    pthread_mutex_lock(&worker_mutex);  // Lock threads out of doing work

    /*
    Loop until some thread is inactive (or the program ends), since global_clock is only run
    when all threads become active.
    */
    while (all_active() && threads_remaining > 0)
    {
        // Signal all waiting threads that it is time for them to be processed
        while (threads_waiting.head != NULL && threads_waiting.head->priority1 <= global_time)
        {
            pthread_cond_signal(&thread_wakeup_conds[pop(&threads_waiting)]);
            // Wait until thread signals it is done processing.
            pthread_cond_wait(&ready, &worker_mutex);
        }

        // Never make decisions if some data isn't arrived
        if (all_active())
        {
            global_time++; // Time is integral, so next action must come at least 1 later
            signal_io();
            signal_cpu();
        }
    }
    pthread_mutex_unlock(&worker_mutex);
    pthread_mutex_unlock(&process_mutex);
}

// Debugging purposes only
void print_queue(struct priority_queue *queue)
{
    struct priority_node *current = queue->head;
    while (current != NULL)
    {
        printf("%d ", current->tid);
        current = current->next;
    }
    printf("\n");
}