
const int MAX_THREADS = 4;

struct registers {
	unsigned long rax, rbx, rcx, rdx, rsi, rdi, rbp, rsp, r8, r9, r10, r11, r12, r13, r14, r15;
	unsigned long long rip;
};

enum STATE {
	READY = 0,
	RUNNING,
	DONE

};

typedef struct {
    void *(*func)(void *);
	void * arg;
	void * stack;
	registers reg;
	int id;
	int priority;
	bool running;	
	enum STATE state;
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
