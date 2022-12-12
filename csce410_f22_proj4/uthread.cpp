/* Write your core functionality in this file. */

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <list>
#include <climits>
#include <iostream>

#include "uthread.h"

std::list<uthread_t*> ready_queue;

enum uthread_policy POLICY = UTHREAD_DIRECT_PTHREAD;

int num_threads_exited = 0;
int num_threads = 0;

unsigned long long rip;
__thread uthread_t* current_uthread;
__thread bool just_context_switched = false;

pthread_mutex_t queue_lock;
pthread_mutex_t texited_lock;
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
		if ( !ready_queue.empty() ) {

			pthread_mutex_lock(&queue_lock);

			__asm__ __volatile__ (
                                "lea 0(%%rip), %0\n"
                                :"=r"(rip)
                        );

			/* Save the current register values using assembly code. */

			// Select a user-space thread from the ready queue 
			uthread_t* t = find_lowest_priority_thread();
			current_uthread = t;
			t->reg.rip = rip;

                	// Run the task of the user-space thread
			if( !t->running ) {
			        t->running = true;
				pthread_mutex_unlock(&queue_lock);
				
				unsigned long rsp = 0;
				unsigned long rbp = 0;
				
				__asm__ __volatile__(
					"mov %%rsp, %0\n"
					"mov %%rbp, %1\n"
					: "=r"(rsp), "=r"(rbp)
				);
	
				__asm__ __volatile__(
                			// Inline assembly code to switch to the target user stack. 
					"mov %0, %%rbp\n"
					"mov %0, %%rsp\n"
					"mov %1, %%rax\n"
					"mov %2, %%rdi\n"
					"call *%%rax\n"	
					
					// Inline assembly to reset the rsp and rbp registers
					"mov %3, %%rbp\n"
					"mov %4, %%rsp\n"
					:
					:"r" ((unsigned long)&t->stack + 4096)
					,"r" (t->func), "r"(t->arg), "r"(rbp), "r"(rsp)
					:"memory"
				);
			}
			else {
      				printf("Switching to target context.....\n");
				just_context_switched = true; 
				pthread_mutex_unlock(&queue_lock);		
		
				// If this is NOT the first time the uthread is running, do the context switch 
	
				 __asm__ __volatile__(
					// Inline assembly code to switch to the target context
					"mov %0, %%rbx\n"
					"mov %1, %%rcx\n"
					"mov %2, %%rdx\n"
			                "mov %3, %%rsi\n"
                			"mov %4, %%rdi\n"
                			"mov %5, %%rbp\n"
                			"mov %6, %%rsp\n"
                			"mov %7, %%r8\n"
                			"mov %8, %%r9\n"
                			"mov %9, %%r10\n"
                			"mov %10, %%r11\n"
                			"mov %11, %%r12\n"
                			"mov %12, %%r13\n"
                			"mov %13, %%r14\n"
					:
        			        :"r" (t->reg.rbx),
                			"r" (t->reg.rcx),
                			"r" (t->reg.rdx),
                			"r" (t->reg.rsi),
					"r" (t->reg.rdi),
					"r" (t->reg.rbp),
					"r" (t->reg.rsp),
					"r" (t->reg.r8),
					"r" (t->reg.r9),
					"r" (t->reg.r10),
					"r" (t->reg.r11),
					"r" (t->reg.r12),
					"r" (t->reg.r13),
					"r" (t->reg.r14)
					: "memory"
				);
				
				printf("Func Addr %lu | Arg %lu\n", (unsigned long)&t->reg.rdi, (unsigned long)&t->arg);

				__asm__ __volatile__(
					// Inline assembly code to save r15 register and call function
					"mov %0, %%r15\n"
					"mov %1, %%rax\n"
                                        "mov %2, %%rdi\n"
                                        "call *%%rax\n"
					:
					:"r"(t->reg.r15)
					,"r" (t->func), "r"(t->arg)	
					:"memory"
				);
			}
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
	if (pthread_mutex_init(&texited_lock, NULL)) {
		printf("pthread_mutex_init failed for texited_lock");
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

void uthread_create(void (*func) (void*), void* arg)
{
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

void uthread_exit(void)
{
	pthread_mutex_lock(&texited_lock);
	num_threads_exited += 1;
	pthread_mutex_unlock(&texited_lock);

	pthread_exit(NULL);
}

void uthread_yield(void)
{
	/* Add the current thread back into the queue at the front */
	pthread_mutex_lock(&queue_lock);
	ready_queue.push_front(current_uthread);
	pthread_mutex_unlock(&queue_lock);

	/* Save the current register values using assembly code. */	
	__asm__ __volatile__ (
		"mov %%rbx, %0\n"
		"mov %%rcx, %1\n"
		"mov %%rdx, %2\n"
		"mov %%rsi, %3\n"
		"mov %%rdi, %4\n"
		"mov %%rbp, %5\n"
		"mov %%rsp, %6\n"
		"mov %%r8, %7\n"
		"mov %%r9, %8\n"
		"mov %%r10, %9\n"
		"mov %%r11, %10\n"
		"mov %%r12, %11\n"
		"mov %%r13, %12\n"
		"mov %%r14, %13\n"

		:
		"=r"(current_uthread->reg.rbx),
		"=r"(current_uthread->reg.rcx),
		"=r"(current_uthread->reg.rdx),
		"=r"(current_uthread->reg.rsi),
		"=r"(current_uthread->reg.rdi),
		"=r"(current_uthread->reg.rbp),
		"=r"(current_uthread->reg.rsp),
		"=r"(current_uthread->reg.r8),
		"=r"(current_uthread->reg.r9),
		"=r"(current_uthread->reg.r10),
		"=r"(current_uthread->reg.r11),
		"=r"(current_uthread->reg.r12),
		"=r"(current_uthread->reg.r13),
		"=r"(current_uthread->reg.r14)
		:
	);

	__asm__ __volatile__ (
		"mov %%r15, %0\n"
		:"=r"(current_uthread->reg.r15)
	);

	/* Prevent infinite loop */
	if ( just_context_switched ) {
        	just_context_switched = false;
        	return;
    	}

	/* Switch to the previously saved context of the scheduler */
	__asm__ __volatile__(
		"jmp *%0\n"
		:
		: "r"(current_uthread->reg.rip)
		: "memory"
	);
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
