#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

// test case for mutex: create threads and increment a shared variable that is inside a critical section
// half of the threads increment the count var while the other threads should not touch the count var.
#define COUNTER_FACTOR 100000000
#define THREAD_CNT 4

pthread_mutex_t mutex;

void wait() {

	for(long int i=0; i<COUNTER_FACTOR; i++);

        return;
}

void *counter() {
    unsigned long int c = 10;
	int i;
	for (i = 0; i < c; i++) {
		if ((i % 100000) == 0) {
			// printf("id: 0x%lx counted to %d of %ld\n",
			//        pthread_self(), i, c);
		}
	}
	return NULL;
}

int count = 0;

void *thread_mutex() {
	 
        // mutex lock
	if(pthread_mutex_lock(&mutex) == EBUSY) {
                printf("pthread_mutex_lock: busy\n");
        }
        
        printf("WITH mutex lock: \n");
	printf("%lx, before wait count: %i\n", pthread_self(), count);

        // the magical count var
	count++;

	wait();

        printf("WITH mutex lock: \n");
	printf("%lx, after wait count: %i\n", pthread_self(), count);
        printf("\n");

        // mutex unlock
	pthread_mutex_unlock(&mutex);

	return NULL;
}

int main() {
	if(pthread_mutex_init(&mutex, NULL) != 0) {
                printf("error: init failed\n");
        }
        pthread_t threads[THREAD_CNT];

        // creating threads
	pthread_create(&threads[0], NULL, &thread_mutex, (void *) (intptr_t) 0);
	pthread_create(&threads[0], NULL, counter, (void *) (intptr_t) 0);

        pthread_create(&threads[1], NULL, &thread_mutex, (void *) (intptr_t) 1);
	pthread_create(&threads[1], NULL, counter, (void *) (intptr_t) 1);

        pthread_create(&threads[2], NULL, &thread_mutex, (void *) (intptr_t) 2);
	pthread_create(&threads[2], NULL, counter, (void *) (intptr_t) 2);

        pthread_create(&threads[3], NULL, &thread_mutex, (void *) (intptr_t) 3);
	pthread_create(&threads[3], NULL, counter, (void *) (intptr_t) 3);

        pthread_create(&threads[4], NULL, &thread_mutex, (void *) (intptr_t) 4);
	pthread_create(&threads[4], NULL, counter, (void *) (intptr_t) 4);

        wait();

        printf("=========\n");
        printf("final value of count: %d\n", count);

        if(pthread_mutex_destroy(&mutex) == 0) {
                printf("destroy successful\n");
        } else if(pthread_mutex_destroy(&mutex) == -1) {
                printf("destroy unsuccessful\n");
        }
	else
	{
		printf("whatishappening");
	}


	return 0;
}
