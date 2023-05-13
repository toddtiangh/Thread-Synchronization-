#include "ec440threads.h"
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
#include <stdint.h>
#include <stdalign.h>

/* You can support more threads. At least support this many. */
#define MAX_THREADS 128

/* Your stack should be this many bytes in size */
#define THREAD_STACK_SIZE 32767

/* Number of microseconds between scheduling events */
#define SCHEDULER_INTERVAL_USECS (50 * 1000)

/* Extracted from private libc headers. These are not part of the public
 * interface for jmp_buf.
 */
#define JB_RBX 0
#define JB_RBP 1
#define JB_R12 2
#define JB_R13 3
#define JB_R14 4
#define JB_R15 5
#define JB_RSP 6
#define JB_PC 7
#define REG_RAX 15


struct thread_control_block thread_list[MAX_THREADS] = {0}; // array to store threads
pthread_t TID = 0; // global thread tid
struct sigaction signal_handler;

static void schedule()
{
	int ljump = 0;

	if(thread_list[TID].state == TS_RUNNING) // If the thread is running we set it to ready then switch to another thread
	{
		thread_list[TID].state = TS_READY;
	}
	pthread_t tid = TID;

	while(1) // we move to the next thread in the array which is TS_READY starting from our last thread TID
	{
		if(tid == MAX_THREADS - 1)
		{
			tid = 0;
		}
		else
		{
			tid++;
		}
		if(thread_list[tid].state == TS_READY)
		{
			break; // break if found
		}
	}
	

	if(thread_list[TID].state != TS_EXITED)
	{
		ljump = setjmp(thread_list[TID].regs); // save the context of the current thread
	}
	
	if(!ljump) // then we switch to the next thread which was saved by setjmp and we use longjmp to load back the state
	{
		TID = tid;
		thread_list[TID].state = TS_RUNNING;
		longjmp(thread_list[TID].regs, 1);
	}
}	

static void scheduler_init()
{	

	for(int i = 0; i < MAX_THREADS; i++) // init threads to standby await scheduling
	{
		thread_list[i].state = TS_STANDBY;
		thread_list[i].thread_id = i;
	}

	struct itimerval interval; // init scheduler to SIGALRM at expiration. 
	interval.it_value.tv_sec = 0; 
	interval.it_value.tv_usec = SCHEDULER_INTERVAL_USECS;
	interval.it_interval.tv_sec = 0;
	interval.it_interval.tv_usec = SCHEDULER_INTERVAL_USECS;
	setitimer(ITIMER_REAL, &interval, NULL); // Counts down in "real time" aka clock time
	
	signal_handler = (struct sigaction) // round robin
	{
		.sa_handler = &schedule,
		.sa_flags = SA_NODEFER,
	};
	sigemptyset(&signal_handler.sa_mask);
	sigaction(SIGALRM, &signal_handler, NULL);
}

void pthread_save() // saves value of start routine and calls pthread_exit
{
	union ucontext 
	{
		void * ptrs[32];
		int ints[32];
	};

	ucontext_t context;
	getcontext(&context);

	void * reg = (void*) context.uc_mcontext.gregs[REG_RAX];
	pthread_exit(reg);
}

int pthread_create(
	pthread_t *thread, const pthread_attr_t *attr,
	void *(*start_routine) (void *), void *arg)
{
	static bool is_first_call = true;
	attr = NULL;
	int first_thread = 0;
	if (is_first_call) // first call init
	{
		scheduler_init();
		is_first_call = false;
		thread_list[0].state = TS_READY;
		first_thread = setjmp(thread_list[0].regs);
	}
	
	if(!first_thread)
	{
		pthread_t thread_tid = 1; // find a valid thread id and break
		
		for(; thread_tid < MAX_THREADS; thread_tid++)
		{
			if(thread_list[thread_tid].state == TS_STANDBY)
			{
				*thread = thread_tid;
				break;
			}
		}
		
		if(thread_tid == MAX_THREADS) // if we at max_threads we return -1
		{
			fprintf(stderr, "MAX_THREADS REACHED, EXITING\n");
			return -1;
		}	

		setjmp(thread_list[thread_tid].regs); // saves the state of our current thread
		
		asm  ( // start thunk- "ing" to PC
				"movq %0, %%rax\n\t"
				"movq %%rax, %1;\n\t"
				:
				: "rm" (ptr_mangle((unsigned long int)start_thunk)), "m" (thread_list[thread_tid].regs[0].__jmpbuf[JB_PC])
				: "%rax", "memory"
		   	     );

		asm  (
				"movq %0, %%rax\n\t" //arg to R13
				"movq %%rax, %1\n\t"
				:
				: "rm"((long) arg), "m"(thread_list[thread_tid].regs[0].__jmpbuf[JB_R13])
				: "%rax", "memory"
			     );

		asm  ( // start_routine to R12
				"movq %0, %%rax\n\t"
 				"movq %%rax, %1\n\t"
				: 
				: "rm" ((unsigned long int) start_routine), "m" (thread_list[thread_tid].regs[0].__jmpbuf[JB_R12])
				: "%rax", "memory"
			     );


		thread_list[thread_tid].stack = malloc(THREAD_STACK_SIZE); // allocate space for stack and move the address of pthread_exit to the top of stack
		void * top_of_stack = thread_list[thread_tid].stack + THREAD_STACK_SIZE; // calculate memory location for the top of the stack
		void * stack_ptr = top_of_stack - sizeof(&pthread_save); // address of the stack_ptr to the top of stack minus the size of pthread_save which will implicitly call pthread_exit
		void (*save)(void*) = (void*) &pthread_save;
		stack_ptr = memcpy(stack_ptr, &save, sizeof(save)); // copy the contents of save to stack_ptr location and then update the pointer to the next available memory location on the stack
		
		asm ( // moves our stack_ptr to the new stack
				"movq %0, %%rax\n\t"
				"movq %%rax, %1\n\t"
				:
				: "rm" (ptr_mangle((unsigned long int) stack_ptr)), "m" (thread_list[thread_tid].regs[0].__jmpbuf[JB_RSP])
				: "%rax", "memory"
			     );

		thread_list[thread_tid].state = TS_READY; // set thread to TS_READY 
		thread_list[thread_tid].thread_id = thread_tid; // set thread ID
		
		schedule(); // add the thread to our round robin scheduler
	}

	else
	{	
		first_thread = 0; 
	}

	return 0;
}

void pthread_exit(void *value_ptr)
{
	thread_list[TID].state = TS_EXITED; // mark thread as exited
	pthread_t tid = thread_list[TID].thread_id;
	if(tid != TID)
	{
		thread_list[tid].state = TS_READY;
	}

	int remaining_threads = 0; // check if any other threads in the list still are running or ready
	for(int i = 0; i < MAX_THREADS; i++)
	{
		if(thread_list[i].state == TS_READY) remaining_threads = 1;
		else if(thread_list[i].state == TS_RUNNING) remaining_threads = 1;
		else if(thread_list[i].state == TS_BLOCKED) remaining_threads = 1;
		else continue;
	}

	if(remaining_threads > 0) // if we find any, then we keep scheduleing 
	{
		schedule();
	}	

	for(int i = 0; i < MAX_THREADS; i++) // free the stack for any exited threads.
	{
		if(thread_list[i].state == TS_EXITED)
		{
			free(thread_list[i].stack);
		}
	}

	exit(0);
}

pthread_t pthread_self(void)
{
	return TID;
}


int pthread_mutex_init(pthread_mutex_t *restrict mutex, const pthread_mutexattr_t *restrict attr)
{
	MutexControlBlock * mcb = (MutexControlBlock *) malloc(sizeof(MutexControlBlock)); // tried doing the union wrapper but did not go well and I have gotten no sleep. So we will do it the easy way
	
    	mcb->locked = 0;
	mcb->waiting_threads = createQueue(MAX_THREADS); // we make a queue, struct details are on header file because scrolling in vim is painful. 

	mutex->__align = (long) mcb;	// align so our struct is inline with our memory space 
   	return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	MutexControlBlock * mcb = (MutexControlBlock *) (mutex->__align); 
	
	if(mcb->locked == 0)
	{
		destroyQueue(mcb->waiting_threads); // this calls the dequeue function which frees each TID node until the queue is empty then frees the queue
	
		free((void *)(mutex->__align)); // free'ing our mutex
		return 0;
	}
	else
	{
		return -1;
	}
}

int pthread_mutex_lock(pthread_mutex_t *mutex) 
{
	lock(); // lock first before anything because race conditions can literally occur in the first if statement.
 	MutexControlBlock * mcb = (MutexControlBlock *) (mutex->__align);
	
	if(mcb->locked == 0) // this one probably is too harmful but the other ones below are
	{	
		mcb->locked = 1;
		unlock(); // if our no one has the lock we can just execute our thread and free the lock 
		return 0;
	}
	else
	{
		thread_list[TID].state = TS_BLOCKED; // if the lock is taken

		enqueue(mcb->waiting_threads, TID); // we add our thread to the tail end of our queue
		
		unlock(); // unlock and switch to another thread
		schedule();
		return EBUSY;
	}
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{	
	MutexControlBlock * mcb = (MutexControlBlock *) (mutex->__align); // rinse and repeat
	
	if(isEmpty(mcb->waiting_threads)) //  probably should lock above this
	{	
		lock(); // if our queue is empty we can just finish executing the thread 
		mcb->locked = 0;
		unlock();
		return 0;
	}
	else
	{				
		lock(); // if our queue is not empty we want to lock 
		pthread_t next_thread;
		next_thread = dequeue(mcb->waiting_threads); // get the first thread at the head of the queue
		mcb->locked = 0;
		thread_list[next_thread].state = TS_READY;	// unlock it and set its state to TS_READY to be put back into the scheduler again
		unlock();
		schedule();

		return 0;
	}
}

int pthread_barrier_init(pthread_barrier_t *restrict barrier, const pthread_barrierattr_t *restrict attr, unsigned count)
{
	if(count == 0)
	{
		return EINVAL;
	}

	BarrierControlBlock * bcb = (BarrierControlBlock *) malloc(sizeof(BarrierControlBlock)); // We initialize a barrier struct, tag means it has reached the barrier and thread_count starts from zero and accumulates as threads reach the barrier
	bcb->barrier_tag = 0;
	bcb->curr_thread = -1;
	bcb->thread_count = 0;
	bcb->threshold = count;

	barrier->__align = (long) bcb; 
	return 0;
}

int pthread_barrier_destroy(pthread_barrier_t *barrier)
{
	lock();
	BarrierControlBlock * bcb = (BarrierControlBlock *) (barrier->__align);
	
	if(bcb->thread_count > bcb->threshold) // this will actually never run. so we can always re-use our barrier
	{
		bcb->curr_thread = -1;
		bcb->thread_count = 0;
		bcb->threshold = 0;
		
		free((void *) (barrier->__align));
		return 0;
	}
	else
	{
		return -1;
	}
	return 0;
}

int pthread_barrier_wait(pthread_barrier_t *barrier)
{

	BarrierControlBlock * bcb = (BarrierControlBlock *) (barrier->__align);
	lock(); // lock for race conditions below.
	bcb->thread_count++;	

	if(bcb->barrier_tag != 1)
	{				
		thread_list[TID].state = TS_BLOCKED; // if the thread has not reached the barrier we block it switch to another thread
		bcb->curr_thread = TID; 
		bcb->barrier_tag = 1; // now we can mark it as reached the barrier

		unlock();
		schedule();
	}
	
	while(bcb->thread_count != bcb->threshold) // while our barrier has not reached its threshold keep schedueling threads will wait here
	{		
		schedule();
	}

	lock(); // lock for race conditions

	if(bcb->barrier_tag == 1)
	{					
		bcb->barrier_tag = 0;
		thread_list[bcb->curr_thread].state = TS_READY; // let it through our barrier now
	}
	unlock();
	schedule();

	if(bcb->curr_thread == TID) // one thread will return pthread_barrier_serial_thread
	{	
		bcb->thread_count = 0;
		return PTHREAD_BARRIER_SERIAL_THREAD;	
	}
	
	else
	{
		return 0; // every other one returns a 0
	}
}

