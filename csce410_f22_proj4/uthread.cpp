/* Write your core functionality in this file. */

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <list>

#include "uthread.h"

std::list<uthread_t*> ready_queue;

enum uthread_policy POLICY = UTHREAD_DIRECT_PTHREAD;

__thread uthread_t* current_uthread;
pthread_mutex_t queue_lock;
pthread_t t1;
pthread_t t2;
pthread_t t3;
pthread_t t4;

uthread_t* find_lowest_priority_thread() {
	printf("FINDING LOWEST PRIORITY.....");
	
	//Find the user thread with lowest priority. Should still enforce FIFO at beginning.
	uthread_t* lowest_pri_thread;
	int low_priority = 5;
	
	std::list<uthread_t*>::iterator it;
	for(it = ready_queue.begin(); it != ready_queue.end(); it++) {
		if((*it)->priority < low_priority) {
			lowest_pri_thread = *it;
			low_priority = (*it)->priority;
		}
	}

	//Remove it from ready queue
	for(it = ready_queue.begin(); it != ready_queue.end(); it++) {
		if(*it == lowest_pri_thread) {
			ready_queue.erase(it);
			break;
		}
	}

	return lowest_pri_thread;
}

void* handler(void* arg) {
        while(true) {
		//Need to lock before reading/writing
		pthread_mutex_lock(&queue_lock);
		
		/* Save the current register values using assembly code. */
        	if( current_uthread ) {
			printf("Saving current register values %lu\n", (unsigned long) &current_uthread->reg);
			__asm__ __volatile__ (
        	    		/* Inline assembly code to save the current register values */
					"mov %%rbx, 0(%0)\n"
                                        "mov %%rcx, 8(%0)\n"
                                        "mov %%rdx, 16(%0)\n"
                                        "mov %%rsi, 24(%0)\n"
                                        "mov %%rdi, 32(%0)\n"
                                        "mov %%rbp, 40(%0)\n"
                                        "mov %%rsp, 48(%0)\n"                               

				:	
				:"r" (&current_uthread->reg)
        		);
			printf("Finished saving current regsiter values...");
		}
		
		//Remove from ready queue to be run if non-empty
		if ( !ready_queue.empty() ) {
	        	// Select a user-space thread from the ready queue 
			uthread_t* t =  ready_queue.back();
			ready_queue.pop_back();
			current_uthread = t;
	 
                	// Run the task of the user-space thread
			if( !t->running ) {
				//printf("Before: %lu \n", (unsigned long)&t->stack );

				__asm__ __volatile__(
                			// Inline assembly code to switch to the target user stack 
					"mov %%rbp, %0\n"
					"mov %%rsp, %1\n"	
					:
					:"r" ((unsigned long)&t->stack + 4096), "r" ((unsigned long)&t->stack + 4096)
					:"memory"
				);
			
				//printf("After: %lu \n", (unsigned long)&t->stack );	
				t->running = true;
			}
			else {
				printf("Switch to target context...\n");
				
				//NEED TO SWAP BACK TO ORIGINAL 
				__asm__ __volatile__(
					//Inline assembly code to switch to the target context
					"mov 0(%0), %%rbx\n"
                                        "mov 8(%0), %%rcx\n"
                                        "mov 16(%0), %%rdx\n"
                                        "mov 24(%0), %%rsi\n"
                                        "mov 32(%0), %%rdi\n"
                                        "mov 40(%0), %%rbp\n"
                                        "mov 48(%0), %%rsp\n"
                                	:
                        		:"r" (&t->reg)
					:"memory"
				);
			}
			
			pthread_mutex_unlock(&queue_lock);
			
			printf("Running function.....\n");	
			t->func(t->arg);		
		}
		
        }

        return NULL;
}

void uthread_init(void)
{
	//Set the schedulers policy
	uthread_set_policy(UTHREAD_PRIORITY);
	
	//Initialize the mutex
	if (pthread_mutex_init(&queue_lock, NULL)) {
		printf("pthread_mutex_init failed for queue_lock");
		return;
	}

	//Create our four kernel threads
	pthread_create(&t1, NULL, handler, NULL);
	pthread_create(&t2, NULL, handler, NULL);
	pthread_create(&t3, NULL, handler, NULL);
	pthread_create(&t4, NULL, handler, NULL);
}

void uthread_create(void (*func) (void*), void* arg)
{
	void* stack = malloc(4096);	
	registers r = {};

	// Alloc the user thread
	uthread_t* ut = (uthread_t*) malloc(sizeof(uthread_t));
	ut->func = (void *(*)(void *)) func;
	ut->arg = arg;
	ut->priority = 0;
	ut->stack = stack;
	ut->reg = r;
	ut->running = false;

	// Add to queue with a lock
	pthread_mutex_lock(&queue_lock);
	ready_queue.push_front(ut);	
	pthread_mutex_unlock(&queue_lock);
	
	printf("UTHREAD_CREATE ADDED TO QUEUE. SIZE OF %lu\n", ready_queue.size());	
}

void uthread_exit(void)
{
	printf("uthread_exit\n");
	pthread_exit(NULL);
}

void uthread_yield(void)
{
	//Enqueue to the ready queue with a lower priority than before 
	pthread_mutex_lock(&queue_lock);

	uthread_t* t = ready_queue.front();
	ready_queue.pop_front();
	ready_queue.push_front(current_uthread);

	//Yield
	/* Save the current register values using assembly code. */
    	__asm__ __volatile__ (

        	/* Inline assembly code to save the current register values */
		"mov %%rbx, 0(%0)\n"
                "mov %%rcx, 8(%0)\n"
                "mov %%rdx, 16(%0)\n"
                "mov %%rsi, 24(%0)\n"
                "mov %%rdi, 32(%0)\n"
                "mov %%rbp, 40(%0)\n"
                "mov %%rsp, 48(%0)\n"
                :
                :"r" (&current_uthread->reg)
    	);

    	/* Switch to the previously saved context of the scheduler */

    	__asm__ __volatile__(

        	/* Inline assembly code to switch to the target context */
		"mov 0(%0), %%rbx\n"
                "mov 8(%0), %%rcx\n"
                "mov 16(%0), %%rdx\n"
                "mov 24(%0), %%rsi\n"
                "mov 32(%0), %%rdi\n"
                "mov 40(%0), %%rbp\n"
                "mov 48(%0), %%rsp\n"
                :
                :"r" (&t->reg)
    	);

	//Need code to have code to allocate stack

	 pthread_mutex_unlock(&queue_lock);
}

void uthread_cleanup(void)
{
	pthread_join(t1, NULL);
	pthread_join(t2, NULL);
	pthread_join(t3, NULL);
	pthread_join(t4, NULL);
}

void uthread_set_policy(enum uthread_policy policy)
{
	POLICY = policy;
}

void uthread_set_param(int param)
{
	
}
