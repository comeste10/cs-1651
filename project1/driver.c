#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include "locking.h"

#define ITERATIONS 1000

pthread_barrier_t barrier;
pthread_barrier_t barrier_less_one;
int global_value = 0;
int num_threads;
int producers;

void * ex1_sub_fn(void * ptr) {
    int i = 0;
    
    pthread_barrier_wait(&barrier);

    for (i = 0; i < ITERATIONS; i++) {
        atomic_sub(&global_value, 1);
    }

    return NULL;
}

void * ex2_fn(void * ptr) {
    struct barrier * test_barrier = ptr;
    int i = 0;
    void * ret = NULL;

    for (i = 0; i < ITERATIONS; i++) {
        //  printf("Iter %d, global_value =%d\n", i, bar_data->global_value);

        if (global_value != (num_threads * i)) {
            ret = (void *)-1;
        }
        
        barrier_wait(test_barrier);
            
        atomic_add(&global_value, 1);

        barrier_wait(test_barrier);
    }

    return ret;
}

void * ex3_fn(void * ptr) {
    struct spinlock * lock = ptr;
    int i = 0;
    
    pthread_barrier_wait(&barrier);

    for (i = 0; i < ITERATIONS; i++) {
        spinlock_lock(lock);
        global_value += 1;
        spinlock_unlock(lock);
    }

    return NULL;
}



void * ex4_read_fn(void * ptr) {
    struct read_write_lock * lock = ptr;
    void * ret = NULL;
    int i = 0;
    int j = 0;
    int prev_value = global_value;

    rw_read_lock(lock);
    pthread_barrier_wait(&barrier);
    rw_read_unlock(lock);

    pthread_barrier_wait(&barrier);

    for (i = 0; i < ITERATIONS / 10; i++) {
	//	printf("Read Lock %d\n", i);
	rw_read_lock(lock);
	
       	for (j = 0; j < ITERATIONS / 10; j++) {

	    if (prev_value > global_value) {
		printf("Error: (prev=%d), global=%d\n", prev_value, global_value);
		ret = (void *)-1;
	    }

	    prev_value = global_value;
	}

	//	printf("Read Unlock\n");
	rw_read_unlock(lock);

	usleep(250000);
	
    }

    return ret;
}


void * ex4_write_fn(void * ptr) {
    struct read_write_lock * lock = ptr;
    void * ret = NULL;
    int i = 0;

    pthread_barrier_wait(&barrier);
    pthread_barrier_wait(&barrier);
    
    for (i = 0; i < ITERATIONS / 10; i++) {

	//	printf("Write Lock  %d\n", i);
       	rw_write_lock(lock);       
	
	global_value = 0;

	usleep(250000);
	global_value += i;

	//	printf("Write unlock\n");
	rw_write_unlock(lock);

    }


    return ret;
}


void * enqueue_fn(void * ptr) {
    struct lf_queue * queue = ptr;
    int i;

    for (i = 0; i < ITERATIONS; i++) {
        lf_enqueue(queue, i);
    }

    return 0;

}

// Consumer
void * dequeue_fn(void * ptr) {
    struct lf_queue * queue = ptr;
    int i;

    for (i = 0; i < (ITERATIONS * (num_threads - 1)); i++) {
	int val = 0;

	while (lf_dequeue(queue, &val) != 0);

    }

    return 0;
}

int main(int argc, char ** argv) {
    pthread_t * threads = NULL;
    pthread_attr_t * attrs = NULL;
    long num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    int x = 0;
    cpu_set_t cpuset;
    

    printf("num_cpus=%d\n", num_cpus);

    num_threads = num_cpus;

    threads = malloc(sizeof(pthread_t) * num_threads);
    attrs = malloc(sizeof(pthread_attr_t) * num_threads);

  
    for (x = 0; x < num_threads; x++) {
	CPU_ZERO(&cpuset);
	CPU_SET(x, &cpuset);

	pthread_attr_init(&attrs[x]);
	pthread_attr_setaffinity_np(&attrs[x], sizeof(cpuset), &cpuset);
    }

    pthread_barrier_init(&barrier, NULL, num_threads);
    pthread_barrier_init(&barrier_less_one, NULL, num_threads - 1);

    /* Exercise 1: Simple atomic operations */
    printf("Exercise 1 (sub):\t");
    fflush(stdout);
    {
        int i = 0;
        global_value = (num_threads * ITERATIONS);

        for (i = 0; i < num_threads; i++) {
            pthread_create(&threads[i], &attrs[i], &ex1_sub_fn, NULL);
            
        }

        for (i = 0; i < num_threads; i++) {
            void * ret = NULL;
            pthread_join(threads[i], &ret);
        }


        if (global_value != 0) {
            printf("ERROR\n");
        } else {
            printf("SUCCESS\n");
        }
    }



    /* Barriers */
    printf("Barrier Test:\t\t");
    fflush(stdout);
    {
        // barrier init
        int i = 0;
        struct barrier test_barrier;
        unsigned long long test_ret = 0;
        global_value = 0;
        
        barrier_init(&test_barrier, num_threads);
        
        for (i = 0; i < num_threads; i++) {
            pthread_create(&threads[i], &attrs[i], &ex2_fn, &test_barrier);
        }
        
        for (i = 0; i < num_threads; i++) {
            void * ret = NULL;
            pthread_join(threads[i], &ret);
            test_ret |= (unsigned long long)ret;
        }
        

        if (test_ret != 0) {
            printf("ERROR\n");
        } else {
            printf("SUCCESS\n");
        }
    }

    /* Spinlocks */
    printf("Spinlocks:\t\t");
    fflush(stdout);
    {
        int i = 0;
        struct spinlock lock;

        global_value = 0;

        spinlock_init(&lock);
        
        for (i = 0; i < num_threads; i++) {
            pthread_create(&threads[i], &attrs[i], &ex3_fn, &lock);
        }
        
        for (i = 0; i < num_threads; i++) {
            void * ret = NULL;
            pthread_join(threads[i], &ret);
        }
        

        if (global_value != ITERATIONS * num_threads) {
            printf("ERROR\n");
        } else {
            printf("SUCCESS\n");
        }
    }


    /* Reader/writer Locks */
    printf("Reader Writer Locks:\t");
    fflush(stdout);
    {
        int i = 0;
        struct read_write_lock lock;
	unsigned long long test_ret = 0;

        global_value = 0;

	rw_lock_init(&lock);
        
        for (i = 0; i < num_threads - 1; i++) {
            pthread_create(&threads[i], &attrs[i], &ex4_read_fn, &lock);
        }
        
	pthread_create(&threads[num_threads - 1], &attrs[num_threads - 1], &ex4_write_fn, &lock);

        for (i = 0; i < num_threads; i++) {
            void * ret = NULL;
            pthread_join(threads[i], &ret);
	    test_ret |= (unsigned long long)ret;
        }
	


        if (test_ret != 0) {
            printf("ERROR\n");
        } else {
            printf("SUCCESS\n");
        }
    }


    /* Lock-free queue */
    printf("Lock Free Queue:\t");
    fflush(stdout);
    {
        int i = 0;
        struct lf_queue queue;
	unsigned long long test_ret = 0;


	lf_queue_init(&queue);
        
        for (i = 0; i < num_threads - 1; i++) {
            pthread_create(&threads[i], &attrs[i], &enqueue_fn, &queue);
        }
        
	pthread_create(&threads[num_threads - 1], &attrs[num_threads - 1], &dequeue_fn, &queue);

        for (i = 0; i < num_threads; i++) {
            void * ret = NULL;
            pthread_join(threads[i], &ret);
	    test_ret |= (unsigned long long)ret;
        }
	

        if (test_ret != 0) {
            printf("ERROR\n");
        } else {
            printf("SUCCESS\n");
        }
    }

}
