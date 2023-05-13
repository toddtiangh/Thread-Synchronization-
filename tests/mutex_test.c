#include <stdint.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

/* test case for mutexes: locking a critical section that increments a shared variable between threads */

/* How many threads (aside from main) to create */
#define THREAD_CNT 3

pthread_mutex_t mutex;
int sharedVar = 0;

void* incrementSharedVar(void* arg){

    for(int i=0; i<1000; i++){
        // lock mutex before entering critical section
        pthread_mutex_lock(&mutex);

        // critical section: increment shared variable
        sharedVar++;

        // unlock mutex upon leaving critical section
        pthread_mutex_unlock(&mutex);
    }

    return NULL;

}


int main(int argc, char **argv) {
	
   pthread_mutex_init(&mutex, NULL);

    /* create threads that increment the shared variable (in critical section) */
    pthread_t threads[THREAD_CNT];
    int rc[THREAD_CNT];
    for(int i=0; i<THREAD_CNT; i++){
        rc[i] = pthread_create(&threads[i], NULL, incrementSharedVar, NULL);
        if (rc[i] != 0) {
            fprintf(stderr, "Error creating thread %d: %s\n", i, strerror(rc[i]));
            exit(1);
        }
    }

    
	for(int i = 0; i<THREAD_CNT; i++) {
        	rc[i] = pthread_join(threads[i], NULL);
        	if (rc[i] != 0) {
            	fprintf(stderr, "Error joining thread %d: %s\n", i, strerror(rc[i]));
            	exit(1);
        	}
    	}

	printf("final value of sharedVar: %d\n",sharedVar);

    pthread_mutex_destroy(&mutex);

	return 0;

}
