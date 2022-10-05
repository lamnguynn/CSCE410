#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>

#define SYS_start_record 326
#define SYS_stop_record  327
#define SYS_start_replay 328

struct syscall_record {
    int syscall_nr;           /* syscall no */

    unsigned long args[6];    /* arguments given by user space */
    long rv;                  /* return value */

};

int start_record(int record_id) {
        int ret;
        asm volatile (

                "syscall"                  // Use syscall instruction
                : "=a" (ret)               // Return value in RAX
                : "0"(SYS_start_record),   // Syscall numer
                "D"(record_id)           // 1st parameter in RDI (D)
                : "rcx", "r11", "memory"); // clobber list (RCX and R11)
        return ret;
}


int stop_record(struct syscall_record *recordbuf, size_t count) {
	int ret;
	asm volatile (
	
		"syscall"
		: "=a" (ret)
		: "0"(SYS_stop_record),
		"D"(recordbuf),
		"S"(count)
		: "rcx", "r11", "memory"
	);

	return ret;	

}


int start_replay(int record_id, struct syscall_record *replay_result, size_t count) {
	int ret;
	asm volatile (
		
		"syscall"
		: "=a" (ret)
		: "0"(SYS_start_replay),
		"D"(record_id),
		"S"(replay_result),
		"d"(count)
		: "rcx", "r11", "memory"
	);
	
	return ret;

}


int main() {
	//Start Recording
	int ret = start_record(32443);
	if(ret < 0) {
		printf("start_record has failed (rv = %d)\n", ret);
	}
	printf("Start Recording... %d\n", ret);

	//Making System Calls
	int fd = open("./test", O_RDONLY, S_IRUSR);
	//write(fd, "Hello World", 11);
	//lseek(fd, SEEK_SET, 0);
	char buf[16];
	//read(fd, &buf, 16);
	close(fd);	
	
	//End Recording
	struct syscall_record record[16];
	printf("Stop Recording... %d\n", stop_record(record, 16));
//	for(int i = 0; i < 16; i++){
//                printf("RECORD%d \n", record[i].syscall_nr);
//        }
	printf("RECORD %d %d \n", record[1].args[1], fd);	
	
	//Start Replay
        struct syscall_record replay[16];
        printf("Start Replay... %d\n", start_replay(32443, replay, 16));
		
	return 0;
}
