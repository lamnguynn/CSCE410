/* This header file defines all the user-space threading functions. */

const int MAX_THREADS = 4;

typedef struct {
	unsigned long rbx, rcx, rdx, rsi, rdi, rbp, rsp;
} registers;

typedef struct {
        void *(*func)(void *);
	void * arg;
	void * stack;
	registers reg;
	int priority;
	bool running;	
} uthread_t;

void uthread_init(void);

void uthread_create(void (*func) (void*), void* arg);

void uthread_exit(void);

void uthread_yield(void);

void uthread_cleanup(void);

enum uthread_policy {
    UTHREAD_DIRECT_PTHREAD = 0,
    UTHREAD_PRIORITY,
    /* Add the next policy */
};

void uthread_set_policy(enum uthread_policy policy);

void uthread_set_param(int param);
