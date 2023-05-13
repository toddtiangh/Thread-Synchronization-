#ifndef __THREADS__
#define __THREADS__

#include <pthread.h>
#include <sys/time.h>
#include <signal.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <ucontext.h>
#include <sys/ucontext.h>
#include <limits.h>


/*
 * This file is derived from code provided by Prof. Egele
 */

/*static unsigned long int ptr_demangle(unsigned long int p)
{
    unsigned long int ret;

    asm("movq %1, %%rax;\n"
        "rorq $0x11, %%rax;"
        "xorq %%fs:0x30, %%rax;"
        "movq %%rax, %0;"
    : "=r"(ret)
    : "r"(p)
    : "%rax"
    );
    return ret;
}*/

static unsigned long int ptr_mangle(unsigned long int p)
{
    unsigned long int ret;

    asm("movq %1, %%rax;\n"
        "xorq %%fs:0x30, %%rax;"
        "rolq $0x11, %%rax;"
        "movq %%rax, %0;"
    : "=r"(ret)
    : "r"(p)
    : "%rax"
    );
    return ret;
}

static void *start_thunk() {
  asm("popq %%rbp;\n"           //clean up the function prologue
      "movq %%r13, %%rdi;\n"    //put arg in $rdi
      "pushq %%r12;\n"          //push &start_routine
      "retq;\n"                 //return to &start_routine
      :
      :
      : "%rdi"
  );
  __builtin_unreachable();
}

enum thread_status
{
	TS_EXITED,
	TS_RUNNING,
	TS_READY,
	TS_STANDBY,
	TS_BLOCKED
};

struct thread_control_block 
{
	pthread_t thread_id;
	void* stack;
	enum thread_status state;
	jmp_buf regs;
};

static void schedule();

static void scheduler_init();

int pthread_create(
	pthread_t *thread, const pthread_attr_t *attr,
	void *(*start_routine) (void *), void *arg);

void pthread_exit(void *value_ptr);

pthread_t pthread_self(void);

static void lock()
{
	sigset_t signal_set;
	sigemptyset(&signal_set);
	sigaddset(&signal_set, SIGALRM);
	sigprocmask(SIG_BLOCK, &signal_set, NULL);
}

static void unlock()
{
	sigset_t signal_set;
	sigemptyset(&signal_set);
	sigaddset(&signal_set, SIGALRM);
	sigprocmask(SIG_UNBLOCK, &signal_set, NULL);

}

typedef struct Node // node in our queue that holds our thread tid and the next node
{
	pthread_t data;
	struct Node * next;
}Node;

typedef struct Queue // simple queue implementation 
{
	Node * front;
	Node * back;
}Queue;

typedef struct MutexControlBlock // MCB, we only need a queue and a locked state
{
    int locked;
    Queue * waiting_threads;
} MutexControlBlock;

int pthread_mutex_init(pthread_mutex_t *restrict mutex, const pthread_mutexattr_t *restrict attr);

int pthread_mutex_destroy(pthread_mutex_t *mutex);

int pthread_mutex_lock(pthread_mutex_t *mutex);

int pthread_mutex_unlock(pthread_mutex_t *mutex);

typedef struct BarrierControlBlock
{
	char barrier_tag; // tag for when the reach the barrier
	pthread_t curr_thread; // current calling thread
	unsigned int thread_count; // count of threads at the barrier
	unsigned int threshold;   // input threshold for our barrier
} BarrierControlBlock;

int pthread_barrier_init(pthread_barrier_t *restrict barrier, const pthread_barrierattr_t *restrict attr, unsigned count);

int pthread_barrier_destroy(pthread_barrier_t *barrier);

int pthread_barrier_wait(pthread_barrier_t *barrier);

Queue * createQueue(unsigned size) // standard queue data structure functionalities (thank you geeksforgeeks)
{
	Queue * queue = (Queue *) malloc(sizeof(Queue));
	queue->front = NULL;
	queue->back = NULL;
	return queue;
}

bool isEmpty(Queue * queue)
{

	bool isEmpty = (queue->front == NULL);
	return isEmpty;
}

bool enqueue(Queue * queue, pthread_t tid) // this just adds a thread to the end of the queue
{
	Node * newNode = (Node *) malloc(sizeof(Node));
	newNode->data = tid;
	newNode->next = NULL;

	if(queue->back == NULL)
	{
		queue->front = queue->back = newNode;
	}
	else
	{
		queue->back->next = newNode;
		queue->back = newNode;
	}
	return true;
}

pthread_t dequeue(Queue * queue) // this takes the first node in our queue and returns the thread tid and then frees the node
{
//	pthread_mutex_lock(&(queue->mutex));
	if(isEmpty(queue))
	{
//		pthread_mutex_unlock(&(queue->mutex));
		return -1;
	}

	Node * temp = queue->front;
	pthread_t tid = temp->data;
	queue->front = temp->next;

	if(queue->front == NULL)
	{
		queue->back = NULL;
	}

	free(temp);
//	pthread_mutex_unlock(&(queue->mutex));
	return tid;
}

void destroyQueue(Queue * queue) // bye bye queue
{
//	pthread_mutex_lock(&(queue->mutex));
	while(!isEmpty(queue))
	{
		dequeue(queue);
	}
//	pthread_mutex_unlock(&(queue->mutex));
//	pthread_mutex_destroy(&(queue->mutex));
	free(queue);
}

#endif
