/* Write your core functionality in this file. */

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <list>
#include <climits>
#include <iostream>

#include "uthread.h"

std::list<uthread_t*> ready_queue;
std::list<uthread_t*> ut_exited;

enum uthread_policy POLICY = UTHREAD_DIRECT_PTHREAD;

// Shared variables
int num_threads_exited = 0;
int num_threads = 0;
unsigned long long ripY;
unsigned long long ripH;

// Thread private variables
__thread uthread_t* current_uthread;
__thread bool just_context_switched = false;
__thread registers handler_reg;

// Locks
pthread_mutex_t queue_lock;
pthread_mutex_t texited_lock;
pthread_mutex_t nt_lock;

// Pthreads
pthread_t t1;
pthread_t t2;
pthread_t t3;
pthread_t t4;

uthread_t* find_lowest_priority_thread() {
	//Find the user thread with lowest priority. Should still enforce FIFO at beginning.
	uthread_t* lowest_pri_thread;
	int low_priority = INT_MAX;

	std::list<uthread_t*>::iterator it;
	for(it = ready_queue.begin(); it != ready_queue.end(); it++) {
		if((*it)->priority < low_priority) {
			lowest_pri_thread = *it;
			low_priority = (*it)->priority;
		}
	}

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
			if ( !ready_queue.empty() ) {
				
				/* Save the current register values using assembly code. */
				__asm__ __volatile__ (
					"mov %%rbx, 8(%0)\n"
					"mov %%rcx, 16(%0)\n"
					"mov %%rdx, 24(%0)\n"
					"mov %%rsi, 32(%0)\n"
					"mov %%rdi, 40(%0)\n"
					"mov %%rsp, 48(%0)\n"
					"mov %%rbp, 56(%0)\n"
					"mov %%r8, 64(%0)\n"
					"mov %%r9, 72(%0)\n"
					"mov %%r10, 80(%0)\n"
					"mov %%r11, 88(%0)\n"
					"mov %%r12, 96(%0)\n"
					"mov %%r13, 104(%0)\n"
					"mov %%r14, 112(%0)\n"
					"mov %%r15, 120(%0)\n"
					:
					: "a"(&handler_reg)
					:
				);
		
				__asm__ __volatile__ (
						"lea 0(%%rip), %0\n"
						:"=a"(ripH)
				);

				// Select a user-space thread from the ready queue 
				pthread_mutex_lock(&queue_lock);
				uthread_t* t = find_lowest_priority_thread();
				current_uthread = t;

				// Run the task of the user-space thread
				if( !t->running ) {
					t->running = true;
					pthread_mutex_unlock(&queue_lock);
					
					__asm__ __volatile__(
						// Inline assembly code to switch to the target user stack. 
						"mov %0, %%rsp\n"
						"mov %0, %%rbp\n"
						"mov %1, %%rdi\n"
						"call *%2\n"

						:
						:"r" ((unsigned long)t->stack + 4096)
						,"r" (t->arg), "r"(t->func)
					);

					__asm__ __volatile__(
							//Restore the register of the handler
							"mov 8(%0), %%rbx\n"
							"mov 16(%0), %%rcx\n"
							"mov 24(%0), %%rdx\n"
							"mov 32(%0), %%rsi\n"
							"mov 40(%0), %%rdi\n"
							"mov 48(%0), %%rbp\n"
							"mov 56(%0), %%rsp\n"
							"mov 64(%0), %%r8\n"
							"mov 72(%0), %%r9\n"
							"mov 80(%0), %%r10\n"
							"mov 88(%0), %%r11\n"
							"mov 96(%0), %%r12\n"
							"mov 104(%0), %%r13\n"
							"mov 112(%0), %%r14\n"
							"mov 120(%0), %%r15\n"
							:
							:"a" (&handler_reg)
							:
					);

					//Exit after the function returns
				}
				else {
					pthread_mutex_unlock(&queue_lock);
					
					just_context_switched = true;

					// If this is NOT the first time the uthread is running, do the context switch 
					__asm__ __volatile__(
						// Inline assembly code to switch to the target context
						"mov 8(%0), %%rbx\n"
						"mov 16(%0), %%rcx\n"
						"mov 24(%0), %%rdx\n"
						"mov 32(%0), %%rsi\n"
						"mov 40(%0), %%rdi\n"
						"mov 48(%0), %%rbp\n"
						"mov 56(%0), %%rsp\n"
						"mov 64(%0), %%r8\n"
						"mov 72(%0), %%r9\n"
						"mov 80(%0), %%r10\n"
						"mov 88(%0), %%r11\n"
						"mov 96(%0), %%r12\n"
						"mov 104(%0), %%r13\n"
						"mov 112(%0), %%r14\n"
						"mov 120(%0), %%r15\n"
						:
						:"a" (&t->reg)
						:
					);
					//printf("Jumping to yield() %llu\n", ripY);
					__asm__ __volatile__(
						"jmp *%0\n"
						:
						: "a"(ripY)
						:
					);
				}
			}
        }

        return NULL;
}

void uthread_yield(void) {
	/* Save the current register values using assembly code. */	
	__asm__ __volatile__ (
			"mov %%rbx,  8(%0)\n"
			"mov %%rcx, 16(%0)\n"
			"mov %%rdx, 24(%0)\n"
			"mov %%rsi, 32(%0)\n"
			"mov %%rdi, 40(%0)\n"
			"mov %%rsp, 48(%0)\n"
			"mov %%rbp, 56(%0)\n"
			"mov %%r8, 64(%0)\n"
			"mov %%r9, 72(%0)\n"
			"mov %%r10, 80(%0)\n"
			"mov %%r11, 88(%0)\n"
			"mov %%r12, 96(%0)\n"
			"mov %%r13, 104(%0)\n"
			"mov %%r14, 112(%0)\n"
			"mov %%r15, 120(%0)\n"
			: 
			: "a"(&current_uthread->reg)
			:
	);

	/* Add the current thread back into the queue at the front */
	pthread_mutex_lock(&queue_lock);
	ready_queue.push_front(current_uthread);
	pthread_mutex_unlock(&queue_lock);

	/* Prevent infinite loop */
	__asm__ __volatile__ (
		"lea 0(%%rip), %0\n"
		:"=a"(ripY)
	);

	if ( just_context_switched ) {
		just_context_switched = false;
		return;
	}

	/* Switch to the previously saved context of the scheduler */
	__asm__ __volatile__(
		// Inline assembly code to switch to the target context of handler
			"mov 8(%0), %%rbx\n"
			"mov 16(%0), %%rcx\n"
			"mov 24(%0), %%rdx\n"
			"mov 32(%0), %%rsi\n"
			"mov 40(%0), %%rdi\n"
			"mov 48(%0), %%rbp\n"
			"mov 56(%0), %%rsp\n"
			"mov 64(%0), %%r8\n"
			"mov 72(%0), %%r9\n"
			"mov 80(%0), %%r10\n"
			"mov 88(%0), %%r11\n"
			"mov 96(%0), %%r12\n"
			"mov 104(%0), %%r13\n"
			"mov 112(%0), %%r14\n"
			"mov 120(%0), %%r15\n"
			:
			:"a" (&handler_reg)
			:
	);

	__asm__ __volatile__(
		"jmp *%0\n"
		:	
		: "a"(ripH)
		: 
	); 
}

void uthread_exit(void) {
	//Remove from queue
}

void uthread_init(void) {
	//Set the schedulers policy
	uthread_set_policy(UTHREAD_PRIORITY);
	
	//Initialize the mutex
	if (pthread_mutex_init(&queue_lock, NULL)) {
		printf("pthread_mutex_init failed for queue_lock");
		return;
	}
	if (pthread_mutex_init(&texited_lock, NULL)) {
		printf("pthread_mutex_init failed for texited_lock");
		return;
	}
	if (pthread_mutex_init(&nt_lock, NULL)) {
			printf("pthread_mutex_init failed for nt_lock");
			return;
     }

	//Create our four kernel threads
	if(POLICY == UTHREAD_PRIORITY) {
		pthread_create(&t1, NULL, handler, NULL);
		pthread_create(&t2, NULL, handler, NULL);
		pthread_create(&t3, NULL, handler, NULL);
		pthread_create(&t4, NULL, handler, NULL);
	}
}

void uthread_create(void (*func) (void*), void* arg) {
	void* stack = malloc(4096);	
	registers r = {};
	
	// Alloc the user thread
	uthread_t* ut = (uthread_t*) malloc(sizeof(uthread_t));
	ut->func = (void *(*)(void *)) func;
	ut->arg = arg;
	ut->id = num_threads + 1;
	ut->priority = 0;
	ut->stack = stack;
	ut->reg = r;
	ut->running = false;

	// Add to queue with a lock
	if(POLICY == UTHREAD_DIRECT_PTHREAD) {
		pthread_create(&t1, NULL, ut->func, ut->arg);
			 	
	}
	else {	
		ready_queue.push_front(ut);
		num_threads+=1;	
	}
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
	if (current_uthread) {
		current_uthread->priority = param;	
	}
}
