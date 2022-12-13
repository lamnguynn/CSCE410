/* This is the main body of the test program.
 * DO NOT write any of your core functionality here.
 * You are free to modify this file but be aware that
 * this file will be replace during grading. */

#include <stdio.h>
#include <unistd.h>
#include "uthread.h"

/**
 **************** THREAD FUNCTIONS ****************
 */

void thread(void* arg)
{
    for (int i = 0; i < 10; i++) {
        printf("This is thread %lu\n", (unsigned long) arg);
        usleep(1000);
    }
    uthread_exit();
}

void bar (void* arg) {
    uthread_set_param((unsigned long)arg);
    for (size_t i = 0; i < 10; ++i) {
        usleep(1000);
	printf("This is thread %lu\n", (unsigned long)arg);
        uthread_yield();
    }
    printf("Thread %lu done.\n", (unsigned long)arg);
    uthread_exit();
}

/**
 **************** TEST FUNCTIONS ****************
 */


void test1() {
    uthread_create(thread, (void*)1); 
    uthread_create(thread, (void*)2);
    uthread_create(thread, (void*)3);
    uthread_create(thread, (void*)4);
}

void test2() {
    for (size_t i = 1; i < 4; i++) {
      uthread_create(bar, (void*)i);
    }
}

int main(int argc, const char** argv)
{
	uthread_set_policy(UTHREAD_PRIORITY);
	uthread_init();
	test2();
    uthread_cleanup();

    	return 0;
}
