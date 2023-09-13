# Thread Scheduler

This repository implements a thread scheduler in C with three CPU thread scheduling policies and semaphore support.

## Installation

Clone the repository by running the following command-line prompt:
```
git clone https://github.com/yifan-lu001/thread-scheduler.git
```

Run ```make``` to build the program, and then run ```./proj1 <scheduling-policy> <input-filename>``` to output the Gantt charts for a specific scheduling policy.

Scheduling policies:
```
0 = First Come First Served (FCFS)
1 = Shortest Remaining Time First (SRTF)
2 = Multi-Level Feedback Queue (MLFQ)
```

## Authors

This project was created by Yifan Lu (yifan.lu001@gmail.com) for the CMPSC 473 course at Penn State University.

