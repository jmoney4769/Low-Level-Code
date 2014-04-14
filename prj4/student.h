/*
 * student.h
 * Multithreaded OS Simulation for CS 2200, Project 4
 *
 * YOU WILL NOT NEED TO MODIFY THIS FILE
 *
 */

#ifndef __STUDENT_H__
#define __STUDENT_H__

#include "os-sim.h"

typedef struct node
{
	pcb_t* data;
	struct node* next;
	struct node* prev;
} node;

typedef struct
{
	node* top;
	pthread_mutex_t mutex;
	int size;
} readyQueue;

/* Function declarations */
extern pcb_t* pop(readyQueue* queue);
extern void push(readyQueue* queue, pcb_t* data);
extern readyQueue* initQueue(int cpu_count);

extern void idle(unsigned int cpu_id);
extern void preempt(unsigned int cpu_id);
extern void yield(unsigned int cpu_id);
extern void terminate(unsigned int cpu_id);
extern void wake_up(pcb_t *process);

#endif /* __STUDENT_H__ */
