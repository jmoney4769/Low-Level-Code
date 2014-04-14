/* 
 * hw4 [<nconsumers>]
 * 
 * This problem has you solve the classic "bounded buffer" problem with
 * one producer and multiple consumers:
 *
 *                                       ------------
 *                                    /->| consumer |
 *                                    |  ------------
 *                                    |
 *  ------------                      |  ------------
 *  | producer |-->   bounded buffer --->| consumer |
 *  ------------                      |  ------------
 *                                    |
 *                                    |  ------------
 *                                    \->| consumer |
 *                                       ------------
 *
 *  The program below includes everything but the implementation of the
 *  bounded buffer itself.  main() does this:
 *
 *  1. starts N consumers as per the first argument (default 1)
 *  2. starts the producer
 *
 *  The producer reads positive integers from standard input and passes those
 *  into the buffer.  The consumers read those integers and "perform a
 *  command" based on them (all they really do is sleep for some period...)
 *
 *  on EOF of stdin, the producer passes N copies of -1 into the buffer.
 *  The consumers interpret -1 as a signal to exit.
 */

#define _REENTRANT
#include <stdio.h>
#include <stdlib.h>             /* atoi() */
#include <unistd.h>             /* usleep() */
#include <assert.h>             /* assert() */
#include <signal.h>             /* signal() */
#include <alloca.h>             /* alloca() */
#include <pthread.h>

/**************************************************************************\
 *                                                                        *
 * Bounded buffer.  This is the only part you need to modify.  Your       *
 * buffer should have space for up to 10 integers in it at a time.        *
 *                                                                        *
 * Add any data structures you need (globals are fine) and fill in        *
 * implementations for these three procedures:                            *
 *                                                                        *
 * void buffer_init(void)                                                 *
 *                                                                        *
 *      buffer_init() is called by main() at the beginning of time to     *
 *      perform any required initialization.  I.e. initialize the buffer, *
 *      any mutex/condition variables, etc.                               *
 *                                                                        *
 * void buffer_insert(int number)                                         *
 *                                                                        *
 *      buffer_insert() inserts a number into the next available slot in  *
 *      the buffer.  If no slots are available, the thread should wait    *
 *      (not spin-wait!) for an empty slot to become available.           *
 *                                                                        *
 * int buffer_extract(void)                                               *
 *                                                                        *
 *      buffer_extract() removes and returns the number in the next       *
 *      available slot.  If no number is available, the thread should     *
 *      wait (not spin-wait!) for a number to become available.  Note     *
 *      that multiple consumers may call buffer_extract() simulaneously.  *
 *                                                                        *
 \**************************************************************************/

pthread_mutex_t* mutex;

/* This is pretty much a queue.  Let's make one */
typedef struct bufferElement
{
	int data;
	struct bufferElement* prev;
	struct bufferElement* next;

} bufferElement;

typedef struct bufferQueue
{
	int size;
	bufferElement* current;
	bufferElement* head;
} bufferQueue;

void push(bufferQueue* queue, int data)
{

	bufferElement* node;
	pthread_mutex_lock(mutex);
	if (queue->size >= 10)
	{
		printf("Buffer is full.  This should not have been called");
	}

	node = malloc(sizeof(bufferElement));
	node->data = data;
	if (queue->size) /* if there is anything, put it at the beginning */
	{
		queue->head->prev = node;
		node->next = queue->head;
		queue->head = node;
	}
	else
	{
		queue->current = node;
		queue->head = node;
	}
	queue->size++;
	pthread_mutex_unlock(mutex);
}

int pop(bufferQueue* queue)
{

	bufferElement* current;
	int data;
	pthread_mutex_lock(mutex);

	if (queue->size == 0)
	{
		printf("Queue is empty.  Why did you call this?");
		return 0;
	}

	current = queue->current;
	data = current->data;
	if (queue->size > 1)
	{
		queue->current->prev = queue->current;
		queue->current->next = NULL;
	}
	else
	{
		queue->current = NULL;
		queue->head = NULL;
	}

	queue->size--;
	free(current);
	pthread_mutex_unlock(mutex);
	return data;
}

bufferQueue* queue;
pthread_cond_t bufferFull;
pthread_cond_t bufferEmpty;

void buffer_init(void)
{
	pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER;
	pthread_cond_t cond2 = PTHREAD_COND_INITIALIZER;
	printf("buffer_init called: setting stuff up\n");

	/* Initialize buffer */
	queue = malloc(sizeof(bufferQueue));
	queue->size = 0;
	/* Initialize mutex and conditionals */
	mutex = malloc(sizeof(pthread_mutex_t));
	*mutex = mutex1;

	bufferFull = cond1;
	bufferEmpty = cond2;

	printf("setup finished\n");
}

void buffer_insert(int number)
{
	printf("insert");
	if (queue->size == 10)
	{
		printf("made it\n");
		pthread_cond_broadcast(&bufferFull);
		pthread_cond_wait(&bufferEmpty, mutex);
	}

	printf("buffer_insert called with %d: placing %d into buffer\n", number, number);
	push(queue, number);
	printf("got placed");
}

int buffer_extract(void)
{
	int value;
	printf("extract");
	if (queue->size == 0)
	{
		pthread_cond_broadcast(&bufferEmpty);
		pthread_cond_wait(&bufferFull, mutex);
	}
	value = pop(queue);
	printf("buffer_extract called: returning %d\n", value);
	return value;
}

/**************************************************************************\
 *                                                                        *
 * consumer thread.  main starts any number of consumer threads.  Each    *
 * consumer thread reads and "interprets" numbers from the bounded        *
 * buffer.                                                                *
 *                                                                        *
 * The interpretation is as follows:                                      *
 *                                                                        *
 * o  positive integer N: sleep for N * 100ms                             *
 * o  negative integer:  exit                                             *
 *                                                                        *
 * Note that a thread procedure (called by pthread_create) is required to *
 * take a void * as an argument and return a void * as a result.  We play *
 * a dirty trick: we pass the thread number (main()'s idea of the thread  *
 * number) as the "void *" argument.  Hence the casts.  The return value  *
 * is ignored so we return NULL.                                          *
 *                                                                        *
 \**************************************************************************/

void *consumer_thread(void *raw_consumerno)
{
	int consumerno = (int) raw_consumerno; /* dirty trick to pass in an integer */

	printf("  consumer %d: starting\n", consumerno);
	while (1)
	{
		int number = buffer_extract();

		if (number < 0)
			break;

		usleep(100000 * number); /* "interpret" the command */
		printf("  consumer %d: %d\n", consumerno, number);
		fflush(stdout);
	}

	printf("  consumer %d: exiting\n", consumerno);
	return (NULL);
}

/**************************************************************************\
 *                                                                        *
 * producer.  main calls the producer as an ordinary procedure rather     *
 * than creating a new thread.  In other words the original "main" thread *
 * becomes the "producer" thread.                                         *
 *                                                                        *
 * The producer reads numbers from stdin, and inserts them into the       *
 * bounded buffer.  On EOF from stdin, it finished up by inserting a -1   *
 * for every consumer thread so that all the consumer thread shut down    *
 * cleanly.                                                               *
 *                                                                        *
 \**************************************************************************/

#define MAXLINELEN 128

void producer(int nconsumers)
{
	char buffer[MAXLINELEN];
	int number;

	printf("  producer: starting\n");

	while (fgets(buffer, MAXLINELEN, stdin) != NULL)
	{
		number = atoi(buffer);
		printf("producer: %d\n", number);
		buffer_insert(number);
	}

	printf("producer: read EOF, sending %d '-1' numbers\n", nconsumers);
	for (number = 0; number < nconsumers; number++)
		buffer_insert(-1);

	printf("producer: exiting\n");
}

/*************************************************************************\
 *                                                                       *
 * main program.  Main calls buffer_init and does other initialization,  *
 * fires of N copies of the consumer (N given by command-line argument), *
 * then calls the producer.                                              *
 *                                                                       *
 \*************************************************************************/

int main(int argc, char *argv[])
{
	pthread_t *consumers;
	int nconsumers = 1;
	int kount;

	if (argc > 1)
		nconsumers = atoi(argv[1]);

	printf("main: nconsumers = %d\n", nconsumers);
	assert(nconsumers >= 0);

	/*
	 * 1. initialization
	 */
	buffer_init();
	signal(SIGALRM, SIG_IGN); /* evil magic for usleep() under solaris */

	/*
	 * 2. start up N consumer threads
	 */
	consumers = (pthread_t *) alloca(nconsumers * sizeof(pthread_t));
	for (kount = 0; kount < nconsumers; kount++)
	{
		int test = pthread_create(&consumers[kount], /* pthread number */
		NULL, /* "attributes" (unused) */
		consumer_thread, /* procedure */
		(void *) kount); /* hack: consumer number */

		assert(test == 0);
	}

	/*
	 * 3. run the producer in this thread.
	 */
	producer(nconsumers);

	/*
	 * n. clean up: the producer told all the consumers to shut down (by
	 *    sending -1 to each).  Now wait for them all to finish.
	 */
	for (kount = 0; kount < nconsumers; kount++)
	{
		int test = pthread_join(consumers[kount], NULL);

		assert(test == 0);
	}
	return (0);
}
