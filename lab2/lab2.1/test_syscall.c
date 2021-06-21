#include <stdio.h>
#include <linux/unistd.h>
#include <sys/syscall.h>

typedef struct process_state {
	int pid;
	unsigned long long utime;
	char comm[16];
} process_state;

int main(void) {
    process_state states[1000];
    syscall(549,states);
    printf("PID       TIME/ms     COMMAND\n");
    for (int i = 0; states[i].pid != 0; i++)
        printf("%-10d%-12d%s\n", states[i].pid, states[i].utime, states[i].comm);
    
    return 0;
}
