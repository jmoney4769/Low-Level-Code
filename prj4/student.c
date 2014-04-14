/*
 * student.c
 * Multithreaded OS Simulation for CS 2200, Project 4
 *
 * This file contains the CPU scheduler for the simulation.  
 */

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "os-sim.h"
#include "student.h"

/*
 * current[] is an array of pointers to the currently running processes.
 * There is one array element corresponding to each CPU in the simulation.
 *
 * current[] should be updated by schedule() each time a process is scheduled
 * on a CPU.  Since the current[] array is accessed by multiple threads, you
 * will need to use a mutex to protect it.  current_mutex has been provided
 * for your use.
 */
static pcb_t **current;
static pthread_mutex_t current_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t idleCond = PTHREAD_COND_INITIALIZER;
static readyQueue* queue;

extern pcb_t* pop(readyQueue* queue)
{
	pthread_mutex_lock(&queue->mutex);
	if (queue->top == NULL)
	{
		return 0;
	}

	pcb_t* data = queue->top->data;
	node* top = queue->top;
	queue->top->prev = queue->top;
	queue->top->next = NULL;
	free(top);
	queue->size--;
	pthread_mutex_unlock(&queue->mutex);
	return data;
}

extern void push(readyQueue* queue, pcb_t* data)
{
	pthread_mutex_lock(&queue->mutex);
	node* new = malloc(sizeof(node));
	new->data = data;
	if (queue->top == NULL)
	{
		queue->top = new;
		queue->size++;
		return;
	}
	queue->top->next = new;
	new->prev = queue->top;
	queue->top = new;
	queue->size++;
	pthread_mutex_unlock(&queue->mutex);
}

extern readyQueue* initQueue(int cpu_count)
{
	readyQueue* queue = malloc(sizeof(readyQueue));
	pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;

	queue->top = NULL;
	queue->mutex = mutex1;
	queue->size = 0;

	return queue;
}

/*
 * schedule() is your CPU scheduler.  It should perform the following tasks:
 *
 *   1. Select and remove a runnable process from your ready queue which 
 *	you will have to implement with a linked list or something of the sort.
 *
 *   2. Set the process state to RUNNING
 *
 *   3. Call context_switch(), to tell the simulator which process to execute
 *      next on the CPU.  If no process is runnable, call context_switch()
 *      with a pointer to NULL to select the idle process.
 *	The current array (see above) is how you access the currently running process indexed by the cpu id. 
 *	See above for full description.
 *	context_switch() is prototyped in os-sim.h. Look there for more information 
 *	about it and its parameters.
 */
static void schedule(unsigned int cpu_id)
{
	if (queue->size == 0)
	{
		context_switch(cpu_id, NULL, -1);
		return;
	}
	pcb_t* process = (pcb_t*) pop(queue);
	process->state = PROCESS_RUNNING;
	context_switch(cpu_id, process, -1);
}

/*
 * idle() is your idle process.  It is called by the simulator when the idle
 * process is scheduled.
 *
 * This function should block until a process is added to your ready queue.
 * It should then call schedule() to select the process to run on the CPU.
 */
extern void idle(unsigned int cpu_id)
{
	pthread_cond_wait(&idleCond, &current_mutex);
	schedule(cpu_id);

	/*
	 * REMOVE THE LINE BELOW AFTER IMPLEMENTING IDLE()
	 *
	 * idle() must block when the ready queue is empty, or else the CPU threads
	 * will spin in a loop.  Until a ready queue is implemented, we'll put the
	 * thread to sleep to keep it from consuming 100% of the CPU time.  Once
	 * you implement a proper idle() function using a condition variable,
	 * remove the call to mt_safe_usleep() below.
	 */
}

/*
 * preempt() is the handler called by the simulator when a process is
 * preempted due to its timeslice expiring.
 *
 * This function should place the currently running process back in the
 * ready queue, and call schedule() to select a new runnable process.
 */
extern void preempt(unsigned int cpu_id)
{
	/* FIX ME */
}

/*
 * yield() is the handler called by the simulator when a process yields the
 * CPU to perform an I/O request.
 *
 * It should mark the process as WAITING, then call schedule() to select
 * a new process for the CPU.
 */
extern void yield(unsigned int cpu_id)
{
	current[cpu_id]->state = PROCESS_WAITING;
	schedule(cpu_id);
}

/*
 * terminate() is the handler called by the simulator when a process completes.
 * It should mark the process as terminated, then call schedule() to select
 * a new process for the CPU.
 */
extern void terminate(unsigned int cpu_id)
{
	current[cpu_id]->state = PROCESS_TERMINATED;
	schedule(cpu_id);
}

/*
 * wake_up() is the handler called by the simulator when a process's I/O
 * request completes.  It should perform the following tasks:
 *
 *   1. Mark the process as READY, and insert it into the ready queue.
 *
 *   2. If the scheduling algorithm is static priority, wake_up() may need
 *      to preempt the CPU with the lowest priority process to allow it to
 *      execute the process which just woke up.  However, if any CPU is
 *      currently running idle, or all of the CPUs are running processes
 *      with a higher priority than the one which just woke up, wake_up()
 *      should not preempt any CPUs.
 *	To preempt a process, use force_preempt(). Look in os-sim.h for 
 * 	its prototype and the parameters it takes in.
 */
extern void wake_up(pcb_t *process)
{
	process->state = PROCESS_READY;
	push(queue, process);

}

/*
 * main() simply parses command line arguments, then calls start_simulator().
 * You will need to modify it to support the -r and -p command-line parameters.
 */
int main(int argc, char *argv[])
{
	int cpu_count;

	/* Parse command-line arguments */
	if (argc != 2)
	{
		fprintf(stderr, "CS 2200 Project 4 -- Multithreaded OS Simulator\n"
				"Usage: ./os-sim <# CPUs> [ -r <time slice> | -p ]\n"
				"    Default : FIFO Scheduler\n"
				"         -r : Round-Robin Scheduler\n"
				"         -p : Static Priority Scheduler\n\n");
		return -1;
	}
	cpu_count = atoi(argv[1]);

	/* FIX ME - Add support for -r and -p parameters*/

	/* Allocate the current[] array and its mutex */
	current = malloc(sizeof(pcb_t*) * cpu_count);
	assert(current != NULL);
	queue = initQueue(cpu_count);

	/* Start the simulator in the library */
	start_simulator(cpu_count);

	return 0;
}

