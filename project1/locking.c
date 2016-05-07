// Steve Comer
// CS 1651
// Project 1
// 2 Oct 13

#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#include "locking.h"


/* Exercise 1:
 *     Basic memory barrier
 */

void mem_barrier() {
	asm volatile("" ::: "memory");
}


/* Exercise 2: 
 *     Simple atomic operations 
 */

void atomic_sub( int * value, int dec_val) {
	asm volatile("lock; sub %1, %0"
				: "+m" (*value)				// outputs
				: "r" (dec_val)				// inputs
				: "memory"					// clobber list
				);
}

void atomic_add( int * value, int inc_val) {
	asm volatile("lock; add %1, %0"
				: "+m" (*value)				// outputs
				: "r" (inc_val)				// inputs
				: "memory"					// clobber list
				);
}


/* Exercise 3:
 *     Spin lock implementation
 */


/* Compare and Swap
 * Returns the value that was stored at *ptr when this function was called
 * Sets '*ptr' to 'new' if '*ptr' == 'expected'
 */

unsigned int compare_and_swap(unsigned int * ptr, unsigned int expected,  unsigned int new) {
	asm volatile("lock; cmpxchg %2, %0"
				: "+m" (*ptr), "+a" (expected) 			// outputs
				: "r" (new)								// inputs
				: "memory"								// clobber list
				);
    return expected;
}

void spinlock_init(struct spinlock * lock) {
	lock->value = 1;
}

void spinlock_lock(struct spinlock * lock) {
	mem_barrier();
	while( compare_and_swap(&(lock->value), 1, 0) != 1 ) {
		// while lock was not unlocked, wait
	}
}

void spinlock_unlock(struct spinlock * lock) {
	mem_barrier();
	lock->value = 1;	
}


/* Exercise 4:
 *     Barrier operations
 */


/* return previous value */
int atomic_add_ret_prev(int * value, int inc_val) {
	int prev = 0;
	asm volatile("lock; xadd %0, %1"
				: "=r" (prev), "+m" (*value)		// outputs
				: "0" (inc_val)						// inputs
				: "memory"							// clobber list
				);
	return prev;
}

void barrier_init(struct barrier * bar, int count) {
	bar->currentBarrier = 0;
	bar->arrived = 0;
	bar->total = count;
}

void barrier_wait(struct barrier * bar) {
	
	// store old barrier value
	int oldBarrier = bar->currentBarrier;

	//increment arrived and check role of current thread
	if(atomic_add_ret_prev(&(bar->arrived), 1) == (bar->total - 1)) {
		bar->arrived = 0;
		bar->currentBarrier++;
	}

	// wait until old barrier is removed
	while(oldBarrier == bar->currentBarrier) {
		// busy wait if not all threads have arrived
	}

}


/* Exercise 5:
 *     Reader Writer Locks
 */

void rw_lock_init(struct read_write_lock * lock) {
	lock->active_readers = 0;
	lock->lock = (struct spinlock *)malloc(sizeof(struct spinlock));
	spinlock_init(lock->lock);
}


void rw_read_lock(struct read_write_lock * lock) {
	spinlock_lock(lock->lock);
	atomic_add(&(lock->active_readers), 1);
	spinlock_unlock(lock->lock);
}

void rw_read_unlock(struct read_write_lock * lock) {
	atomic_sub(&(lock->active_readers), 1);
}

void rw_write_lock(struct read_write_lock * lock) {
	spinlock_lock(lock->lock);
	while(lock->active_readers > 0) {
		// wait until no active_readers remain
	}
}


void rw_write_unlock(struct read_write_lock * lock) {
	spinlock_unlock(lock->lock);
}



/* Exercise 6:
 *      Lock-free queue
 *
 * see: Implementing Lock-Free Queues. John D. Valois.
 *
 * The test function uses multiple enqueue threads and a single dequeue thread.
 *  Would this algorithm work with multiple enqueue and multiple dequeue threads? Why or why not?
 */


/* Compare and Swap 
 * Same as earlier compare and swap, but with pointers 
 * Explain the difference between this and the earlier Compare and Swap function
 * 		This function actually operates on pointers, however they are masked by the uintptr_t type
 * 		which allows us to perform integer-style operations on the pointers
 */
uintptr_t compare_and_swap_ptr(uintptr_t * ptr, uintptr_t expected, uintptr_t new) {
    char success = 0;
	asm volatile("lock; cmpxchg %3, %0; sete %1"
				: "+m" (*ptr), "=r" (success)			// outputs
				: "a" (expected), "r" (new)				// inputs
				: "memory"								// clobber list
				);
	return (success == 1) ? (uintptr_t)1 : (uintptr_t)0;
}


void lf_queue_init(struct lf_queue * queue) {
	queue->head = (struct node *)malloc(sizeof(struct node));
	queue->tail = (struct node *)malloc(sizeof(struct node));
	queue->head->next = queue->tail;
	queue->head->value = 0;
	queue->tail->value = 0;
}

void lf_queue_deinit(struct lf_queue * lf) {
	while(lf->head->next != NULL) {
		free(lf->head->next);
	}
	free(lf->head);
}

void lf_enqueue(struct lf_queue * queue, int val) {
	struct node * q = (struct node *)malloc(sizeof(struct node));
	q->value = val;
	q->next = NULL;
	uintptr_t succ = 0;
	struct node * p;
	do {
		p = queue->tail;
		succ = compare_and_swap_ptr((uintptr_t *)&(p->next), (uintptr_t)NULL, (uintptr_t)q);
		if(succ != 1) {
			compare_and_swap_ptr((uintptr_t *)&(queue->tail), (uintptr_t)p, (uintptr_t)p->next);
		}
	} while(succ != 1);
	compare_and_swap_ptr((uintptr_t *)&(queue->tail), (uintptr_t)p, (uintptr_t)q);
}

int lf_dequeue(struct lf_queue * queue, int * val) {
	struct node * p;
	do {
		p = queue->head;
		if(p->next == NULL) {
			//printf("Error: queue empty\n");
			//exit(0);
			return 0;
		}
	} while(compare_and_swap_ptr((uintptr_t *)&(queue->head), (uintptr_t)p, (uintptr_t)p->next) != 1);
	*val = p->next->value;
	//return p->next->value;
	return 1;
}
